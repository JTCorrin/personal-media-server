#ifndef MEDIA_SERVER_LIBRARY_BROWSE_H
#define MEDIA_SERVER_LIBRARY_BROWSE_H

#include "media_server/library/catalog.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * In-memory artist/album index derived from catalog audio items.
 * Artist/album IDs are synthetic and stable for a given catalog content /
 * discovery order (not persisted across rescans yet).
 *
 * Artists: unique non-empty artist names from audio.
 * Albums:  unique (artist, album) pairs with non-empty album from audio.
 * cover_id: catalog image id matched by artist+album (0 if none).
 */

typedef struct browse_index browse_index_t;

typedef struct browse_artist {
    uint32_t id;
    char name[CATALOG_META_MAX];
    size_t album_count;
    size_t track_count;
} browse_artist_t;

typedef struct browse_album {
    uint32_t id;
    char name[CATALOG_META_MAX];
    char artist[CATALOG_META_MAX];
    uint32_t artist_id; /* 0 if artist was empty / not indexed */
    uint32_t cover_id;  /* catalog image id; 0 if none */
    size_t track_count;
    char release_date[CATALOG_DATE_MAX]; /* "" when absent or inconsistent */
    char genre[CATALOG_META_MAX];        /* "" when absent or inconsistent */
    bool release_date_mixed;
    bool genre_mixed;
    char path_name[CATALOG_META_MAX];   /* cover matching after tags/overrides */
    char path_artist[CATALOG_META_MAX];
    bool path_group_mixed;
    char cover_dir[CATALOG_PATH_MAX]; /* common physical parent of album tracks */
} browse_album_t;

typedef struct browse_track_link {
    uint32_t album_id; /* synthetic browse album id */
    uint32_t cover_id; /* catalog image id; 0 if the album has no cover */
} browse_track_link_t;

/* Build from catalog. Returns NULL on OOM or NULL catalog. */
browse_index_t *browse_index_build(const catalog_t *catalog);
void browse_index_destroy(browse_index_t *index);

size_t browse_artist_count(const browse_index_t *index);
const browse_artist_t *browse_artist_get(const browse_index_t *index, size_t i);
const browse_artist_t *browse_artist_find_id(const browse_index_t *index, uint32_t id);

size_t browse_album_count(const browse_index_t *index);
const browse_album_t *browse_album_get(const browse_index_t *index, size_t i);
const browse_album_t *browse_album_find_id(const browse_index_t *index, uint32_t id);

/* Find derived album/cover ids for an audio catalog id. */
bool browse_track_link_find(const browse_index_t *index, uint32_t track_id,
                            browse_track_link_t *out);

/* True if audio item belongs to this album (artist+album name match). */
bool browse_album_owns_item(const browse_album_t *album, const catalog_item_t *item);

#endif /* MEDIA_SERVER_LIBRARY_BROWSE_H */
