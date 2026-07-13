#ifndef MEDIA_SERVER_LIBRARY_CATALOG_H
#define MEDIA_SERVER_LIBRARY_CATALOG_H

#include "media_server/media/kind.h"

#include <stddef.h>
#include <stdint.h>

#define CATALOG_PATH_MAX 512
#define CATALOG_FILENAME_MAX 256
#define CATALOG_META_MAX 256

typedef struct catalog catalog_t;

typedef struct catalog_item {
    uint32_t id;
    media_kind_t kind;
    char path[CATALOG_PATH_MAX];         /* relative to library root */
    char filename[CATALOG_FILENAME_MAX];
    char artist[CATALOG_META_MAX];
    char album[CATALOG_META_MAX];
    char title[CATALOG_META_MAX];
} catalog_item_t;

catalog_t *catalog_create(void);
void catalog_destroy(catalog_t *catalog);

/* Add an item. IDs start at 1 and increase monotonically. Returns 0 on success. */
int catalog_add(catalog_t *catalog, media_kind_t kind, const char *rel_path);

/*
 * Copy a fully-formed item (e.g. from SQLite). Updates next_id to max(next_id, id+1).
 * Returns 0 on success.
 */
int catalog_add_item(catalog_t *catalog, const catalog_item_t *item);

/* Find by relative path. */
const catalog_item_t *catalog_find_path(const catalog_t *catalog, const char *rel_path);

/*
 * Remap ids in fresh so paths that exist in previous keep the same id.
 * New paths get ids from previous's next_id (or 1). Updates fresh->next_id.
 * previous may be NULL (no-op on ids already assigned by catalog_add).
 * Returns 0 on success.
 */
int catalog_adopt_ids(catalog_t *fresh, const catalog_t *previous);

uint32_t catalog_next_id(const catalog_t *catalog);
void catalog_set_next_id(catalog_t *catalog, uint32_t next_id);

size_t catalog_count(const catalog_t *catalog);
const catalog_item_t *catalog_get(const catalog_t *catalog, size_t index);
const catalog_item_t *catalog_find_id(const catalog_t *catalog, uint32_t id);

size_t catalog_count_kind(const catalog_t *catalog, media_kind_t kind);

#endif /* MEDIA_SERVER_LIBRARY_CATALOG_H */
