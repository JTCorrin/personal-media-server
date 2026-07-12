#include "media_server/library/browse.h"

#include "media_server/media/kind.h"

#include <stdlib.h>
#include <string.h>

struct browse_index {
    browse_artist_t *artists;
    size_t artist_count;
    size_t artist_cap;
    browse_album_t *albums;
    size_t album_count;
    size_t album_cap;
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
                                 const char *album_name, uint32_t artist_id)
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
    index->album_count++;
    return album;
}

browse_index_t *browse_index_build(const catalog_t *catalog)
{
    browse_index_t *index = calloc(1, sizeof(*index));
    if (index == NULL) {
        return NULL;
    }

    if (catalog == NULL) {
        return index;
    }

    for (size_t i = 0; i < catalog_count(catalog); i++) {
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
            album = find_album_by_names(index, item->artist, item->album);
            if (album == NULL) {
                uint32_t artist_id = artist != NULL ? artist->id : 0;
                album = add_album(index, item->artist, item->album, artist_id);
                if (album == NULL) {
                    browse_index_destroy(index);
                    return NULL;
                }
                if (artist != NULL) {
                    artist->album_count++;
                }
            }
            album->track_count++;
        }
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

bool browse_album_owns_item(const browse_album_t *album, const catalog_item_t *item)
{
    if (album == NULL || item == NULL || item->kind != MEDIA_KIND_AUDIO) {
        return false;
    }
    return strcmp(album->artist, item->artist) == 0 &&
           strcmp(album->name, item->album) == 0;
}
