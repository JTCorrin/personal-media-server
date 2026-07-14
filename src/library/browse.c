#include "media_server/library/browse.h"

#include "media_server/media/kind.h"
#include "media_server/media/path_meta.h"
#include "media_server/util/path.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct browse_track_link_entry {
    uint32_t track_id;
    browse_track_link_t link;
} browse_track_link_entry_t;

struct browse_index {
    browse_artist_t *artists;
    size_t artist_count;
    size_t artist_cap;
    browse_album_t *albums;
    size_t album_count;
    size_t album_cap;
    browse_track_link_entry_t *track_links;
    size_t track_link_count;
};

static browse_artist_t *find_artist_by_name(browse_index_t *index, const char *name)
{
    for (size_t i = 0; i < index->artist_count; i++) {
        if (strcmp(index->artists[i].name, name) == 0) {
            return &index->artists[i];
        }
    }
    return NULL;
}

static browse_album_t *find_album_by_names(browse_index_t *index, const char *artist,
                                           const char *album)
{
    for (size_t i = 0; i < index->album_count; i++) {
        if (strcmp(index->albums[i].artist, artist) == 0 &&
            strcmp(index->albums[i].name, album) == 0) {
            return &index->albums[i];
        }
    }
    return NULL;
}

static browse_album_t *find_album_by_cover_dir(browse_index_t *index,
                                                const char *image_path)
{
    browse_album_t *match = NULL;
    char image_dir[CATALOG_PATH_MAX];

    if (path_dirname(image_dir, sizeof(image_dir), image_path) != 0 ||
        image_dir[0] == '\0') {
        return NULL;
    }
    for (size_t i = 0; i < index->album_count; i++) {
        browse_album_t *album = &index->albums[i];

        if (album->cover_dir[0] == '\0' ||
            strcmp(album->cover_dir, image_dir) != 0) {
            continue;
        }
        if (match != NULL) {
            return NULL; /* shared physical directory is ambiguous */
        }
        match = album;
    }
    return match;
}

static int update_album_cover_dir(browse_album_t *album,
                                  const catalog_item_t *track)
{
    char track_dir[CATALOG_PATH_MAX];

    if (path_dirname(track_dir, sizeof(track_dir), track->path) != 0) {
        return -1;
    }
    if (album->track_count == 0) {
        memcpy(album->cover_dir, track_dir, strlen(track_dir) + 1);
    } else if (album->cover_dir[0] != '\0' &&
               strcmp(album->cover_dir, track_dir) != 0) {
        char common_dir[CATALOG_PATH_MAX];

        if (path_common_dir(common_dir, sizeof(common_dir), album->cover_dir,
                            track_dir) != 0) {
            return -1;
        }
        memcpy(album->cover_dir, common_dir, strlen(common_dir) + 1);
    }
    return 0;
}

/* Higher is better: cover > folder > front > other. */
static int cover_name_score(const char *filename)
{
    char stem[CATALOG_FILENAME_MAX];
    const char *dot;
    size_t stem_len;
    size_t i;

    if (filename == NULL || filename[0] == '\0') {
        return 0;
    }

    dot = strrchr(filename, '.');
    stem_len = (dot != NULL) ? (size_t)(dot - filename) : strlen(filename);
    if (stem_len == 0 || stem_len >= sizeof(stem)) {
        return 0;
    }

    memcpy(stem, filename, stem_len);
    stem[stem_len] = '\0';
    for (i = 0; i < stem_len; i++) {
        stem[i] = (char)tolower((unsigned char)stem[i]);
    }

    if (strcmp(stem, "cover") == 0) {
        return 3;
    }
    if (strcmp(stem, "folder") == 0) {
        return 2;
    }
    if (strcmp(stem, "front") == 0) {
        return 1;
    }
    return 0;
}

static void link_album_covers(browse_index_t *index, const catalog_t *catalog)
{
    for (size_t i = 0; i < catalog_count(catalog); i++) {
        const catalog_item_t *item = catalog_get(catalog, i);
        browse_album_t *album;
        int score;
        int best;

        if (item == NULL || item->kind != MEDIA_KIND_IMAGE) {
            continue;
        }

        album = find_album_by_cover_dir(index, item->path);
        if (album == NULL && item->album[0] != '\0') {
            album = find_album_by_names(index, item->artist, item->album);
        }
        if (album == NULL && item->album[0] != '\0') {
            for (size_t j = 0; j < index->album_count; j++) {
                browse_album_t *candidate = &index->albums[j];
                if (!candidate->path_group_mixed &&
                    strcmp(candidate->path_artist, item->artist) == 0 &&
                    strcmp(candidate->path_name, item->album) == 0) {
                    album = candidate;
                    break;
                }
            }
        }
        if (album == NULL) {
            continue;
        }

        score = cover_name_score(item->filename);
        if (album->cover_id == 0) {
            album->cover_id = item->id;
            continue;
        }

        /* Prefer higher score; on tie keep the lower catalog id (already set). */
        best = 0;
        {
            const catalog_item_t *cur = catalog_find_id(catalog, album->cover_id);
            if (cur != NULL) {
                best = cover_name_score(cur->filename);
            }
        }

        if (score > best) {
            album->cover_id = item->id;
        } else if (score == best && item->id < album->cover_id) {
            album->cover_id = item->id;
        }
    }
}

static int ensure_artist_cap(browse_index_t *index)
{
    if (index->artist_count < index->artist_cap) {
        return 0;
    }

    size_t new_cap = index->artist_cap == 0 ? 8 : index->artist_cap * 2;
    browse_artist_t *grown =
        realloc(index->artists, new_cap * sizeof(*grown));
    if (grown == NULL) {
        return -1;
    }
    index->artists = grown;
    index->artist_cap = new_cap;
    return 0;
}

static int ensure_album_cap(browse_index_t *index)
{
    if (index->album_count < index->album_cap) {
        return 0;
    }

    size_t new_cap = index->album_cap == 0 ? 8 : index->album_cap * 2;
    browse_album_t *grown = realloc(index->albums, new_cap * sizeof(*grown));
    if (grown == NULL) {
        return -1;
    }
    index->albums = grown;
    index->album_cap = new_cap;
    return 0;
}

static int track_link_cmp(const void *a, const void *b)
{
    const browse_track_link_entry_t *left = a;
    const browse_track_link_entry_t *right = b;

    if (left->track_id < right->track_id) {
        return -1;
    }
    if (left->track_id > right->track_id) {
        return 1;
    }
    return 0;
}

static browse_artist_t *add_artist(browse_index_t *index, const char *name)
{
    browse_artist_t *artist;

    if (ensure_artist_cap(index) != 0) {
        return NULL;
    }

    artist = &index->artists[index->artist_count];
    memset(artist, 0, sizeof(*artist));
    artist->id = (uint32_t)(index->artist_count + 1);
    memcpy(artist->name, name, strlen(name) + 1);
    index->artist_count++;
    return artist;
}

static browse_album_t *add_album(browse_index_t *index, const char *artist_name,
                                 const char *album_name, uint32_t artist_id,
                                 const catalog_item_t *first_track)
{
    browse_album_t *album;

    if (ensure_album_cap(index) != 0) {
        return NULL;
    }

    album = &index->albums[index->album_count];
    memset(album, 0, sizeof(*album));
    album->id = (uint32_t)(index->album_count + 1);
    album->artist_id = artist_id;
    memcpy(album->name, album_name, strlen(album_name) + 1);
    memcpy(album->artist, artist_name, strlen(artist_name) + 1);
    if (first_track != NULL) {
        char ignored_title[CATALOG_META_MAX];
        memcpy(album->release_date, first_track->release_date,
               strlen(first_track->release_date) + 1);
        memcpy(album->genre, first_track->genre, strlen(first_track->genre) + 1);
        (void)media_path_meta(first_track->path, album->path_artist,
                              sizeof(album->path_artist), album->path_name,
                              sizeof(album->path_name), ignored_title,
                              sizeof(ignored_title));
    }
    index->album_count++;
    return album;
}

browse_index_t *browse_index_build(const catalog_t *catalog)
{
    browse_index_t *index = calloc(1, sizeof(*index));
    size_t item_count;

    if (index == NULL) {
        return NULL;
    }

    if (catalog == NULL) {
        return index;
    }

    item_count = catalog_count(catalog);
    if (item_count > 0) {
        index->track_links = calloc(item_count, sizeof(*index->track_links));
        if (index->track_links == NULL) {
            browse_index_destroy(index);
            return NULL;
        }
    }

    for (size_t i = 0; i < item_count; i++) {
        const catalog_item_t *item = catalog_get(catalog, i);
        browse_artist_t *artist = NULL;
        browse_album_t *album = NULL;

        if (item == NULL || item->kind != MEDIA_KIND_AUDIO) {
            continue;
        }

        if (item->artist[0] != '\0') {
            artist = find_artist_by_name(index, item->artist);
            if (artist == NULL) {
                artist = add_artist(index, item->artist);
                if (artist == NULL) {
                    browse_index_destroy(index);
                    return NULL;
                }
            }
            artist->track_count++;
        }

        if (item->album[0] != '\0') {
            char path_artist[CATALOG_META_MAX];
            char path_album[CATALOG_META_MAX];
            char ignored_title[CATALOG_META_MAX];

            album = find_album_by_names(index, item->artist, item->album);
            if (album == NULL) {
                uint32_t artist_id = artist != NULL ? artist->id : 0;
                album = add_album(index, item->artist, item->album, artist_id, item);
                if (album == NULL) {
                    browse_index_destroy(index);
                    return NULL;
                }
                if (artist != NULL) {
                    artist->album_count++;
                }
            }
            if (!album->release_date_mixed &&
                strcmp(album->release_date, item->release_date) != 0) {
                album->release_date[0] = '\0';
                album->release_date_mixed = true;
            }
            if (!album->genre_mixed && strcmp(album->genre, item->genre) != 0) {
                album->genre[0] = '\0';
                album->genre_mixed = true;
            }
            (void)media_path_meta(item->path, path_artist, sizeof(path_artist),
                                  path_album, sizeof(path_album), ignored_title,
                                  sizeof(ignored_title));
            if (!album->path_group_mixed &&
                (strcmp(album->path_name, path_album) != 0 ||
                 strcmp(album->path_artist, path_artist) != 0)) {
                album->path_group_mixed = true;
                album->path_name[0] = '\0';
                album->path_artist[0] = '\0';
            }
            if (update_album_cover_dir(album, item) != 0) {
                browse_index_destroy(index);
                return NULL;
            }
            album->track_count++;
            index->track_links[index->track_link_count].track_id = item->id;
            index->track_links[index->track_link_count].link.album_id = album->id;
            index->track_link_count++;
        }
    }

    link_album_covers(index, catalog);
    for (size_t i = 0; i < index->track_link_count; i++) {
        uint32_t album_id = index->track_links[i].link.album_id;

        if (album_id > 0 && album_id <= index->album_count) {
            index->track_links[i].link.cover_id =
                index->albums[album_id - 1].cover_id;
        }
    }
    if (index->track_link_count > 1) {
        qsort(index->track_links, index->track_link_count,
              sizeof(*index->track_links), track_link_cmp);
    }
    return index;
}

void browse_index_destroy(browse_index_t *index)
{
    if (index == NULL) {
        return;
    }
    free(index->artists);
    free(index->albums);
    free(index->track_links);
    free(index);
}

size_t browse_artist_count(const browse_index_t *index)
{
    return index == NULL ? 0 : index->artist_count;
}

const browse_artist_t *browse_artist_get(const browse_index_t *index, size_t i)
{
    if (index == NULL || i >= index->artist_count) {
        return NULL;
    }
    return &index->artists[i];
}

const browse_artist_t *browse_artist_find_id(const browse_index_t *index, uint32_t id)
{
    if (index == NULL || id == 0) {
        return NULL;
    }
    for (size_t i = 0; i < index->artist_count; i++) {
        if (index->artists[i].id == id) {
            return &index->artists[i];
        }
    }
    return NULL;
}

size_t browse_album_count(const browse_index_t *index)
{
    return index == NULL ? 0 : index->album_count;
}

const browse_album_t *browse_album_get(const browse_index_t *index, size_t i)
{
    if (index == NULL || i >= index->album_count) {
        return NULL;
    }
    return &index->albums[i];
}

const browse_album_t *browse_album_find_id(const browse_index_t *index, uint32_t id)
{
    if (index == NULL || id == 0) {
        return NULL;
    }
    for (size_t i = 0; i < index->album_count; i++) {
        if (index->albums[i].id == id) {
            return &index->albums[i];
        }
    }
    return NULL;
}

bool browse_track_link_find(const browse_index_t *index, uint32_t track_id,
                            browse_track_link_t *out)
{
    size_t low = 0;
    size_t high;

    if (index == NULL || track_id == 0 || out == NULL) {
        return false;
    }

    high = index->track_link_count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        const browse_track_link_entry_t *entry = &index->track_links[mid];

        if (entry->track_id < track_id) {
            low = mid + 1;
        } else if (entry->track_id > track_id) {
            high = mid;
        } else {
            *out = entry->link;
            return true;
        }
    }
    return false;
}

bool browse_album_owns_item(const browse_album_t *album, const catalog_item_t *item)
{
    if (album == NULL || item == NULL || item->kind != MEDIA_KIND_AUDIO) {
        return false;
    }
    return strcmp(album->artist, item->artist) == 0 &&
           strcmp(album->name, item->album) == 0;
}
