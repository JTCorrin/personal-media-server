#include "media_server/library/catalog.h"

#include "media_server/media/path_meta.h"
#include "media_server/util/path.h"

#include <stdlib.h>
#include <string.h>

struct catalog {
    catalog_item_t *items;
    size_t count;
    size_t capacity;
    uint32_t next_id;
};

catalog_t *catalog_create(void)
{
    catalog_t *catalog = calloc(1, sizeof(*catalog));
    if (catalog == NULL) {
        return NULL;
    }

    catalog->next_id = 1;
    return catalog;
}

void catalog_destroy(catalog_t *catalog)
{
    if (catalog == NULL) {
        return;
    }

    free(catalog->items);
    free(catalog);
}

static int ensure_capacity(catalog_t *catalog)
{
    if (catalog->count < catalog->capacity) {
        return 0;
    }

    size_t new_cap = catalog->capacity == 0 ? 8 : catalog->capacity * 2;
    catalog_item_t *grown = realloc(catalog->items, new_cap * sizeof(*grown));
    if (grown == NULL) {
        return -1;
    }
    catalog->items = grown;
    catalog->capacity = new_cap;
    return 0;
}

int catalog_add(catalog_t *catalog, media_kind_t kind, const char *rel_path)
{
    catalog_item_t *item;
    const char *base;
    size_t path_len;

    if (catalog == NULL || rel_path == NULL || rel_path[0] == '\0' ||
        kind == MEDIA_KIND_NONE) {
        return -1;
    }

    path_len = strlen(rel_path);
    if (path_len >= CATALOG_PATH_MAX) {
        return -1;
    }

    base = path_basename(rel_path);
    if (base == NULL || base[0] == '\0' ||
        strlen(base) >= CATALOG_FILENAME_MAX) {
        return -1;
    }

    if (catalog->count == catalog->capacity) {
        if (ensure_capacity(catalog) != 0) {
            return -1;
        }
    }

    item = &catalog->items[catalog->count];
    memset(item, 0, sizeof(*item));
    item->id = catalog->next_id++;
    item->kind = kind;
    memcpy(item->path, rel_path, path_len + 1);
    memcpy(item->filename, base, strlen(base) + 1);

    /*
     * Currently unreachable in practice (media_path_meta only fails on an
     * empty basename, which was rejected above). Note the failure order is
     * deliberate: count has not been incremented yet, so the half-written
     * slot is simply reused by the next add.
     */
    if (media_path_meta(rel_path, item->artist, sizeof(item->artist), item->album,
                        sizeof(item->album), item->title, sizeof(item->title)) != 0) {
        return -1;
    }

    catalog->count++;
    return 0;
}

int catalog_add_item(catalog_t *catalog, const catalog_item_t *item)
{
    catalog_item_t *dst;

    if (catalog == NULL || item == NULL || item->id == 0 ||
        item->kind == MEDIA_KIND_NONE || item->path[0] == '\0') {
        return -1;
    }

    if (ensure_capacity(catalog) != 0) {
        return -1;
    }

    dst = &catalog->items[catalog->count];
    *dst = *item;
    catalog->count++;

    if (item->id >= catalog->next_id) {
        catalog->next_id = item->id + 1;
    }
    return 0;
}

const catalog_item_t *catalog_find_path(const catalog_t *catalog, const char *rel_path)
{
    if (catalog == NULL || rel_path == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < catalog->count; i++) {
        if (strcmp(catalog->items[i].path, rel_path) == 0) {
            return &catalog->items[i];
        }
    }
    return NULL;
}

int catalog_adopt_ids(catalog_t *fresh, const catalog_t *previous)
{
    uint32_t next;
    uint32_t max_id = 0;

    if (fresh == NULL) {
        return -1;
    }

    if (previous == NULL || catalog_count(previous) == 0) {
        return 0;
    }

    next = previous->next_id;
    for (size_t i = 0; i < previous->count; i++) {
        if (previous->items[i].id >= next) {
            next = previous->items[i].id + 1;
        }
    }

    for (size_t i = 0; i < fresh->count; i++) {
        const catalog_item_t *old =
            catalog_find_path(previous, fresh->items[i].path);
        if (old != NULL) {
            fresh->items[i].id = old->id;
        } else {
            fresh->items[i].id = next++;
        }
        if (fresh->items[i].id > max_id) {
            max_id = fresh->items[i].id;
        }
    }

    fresh->next_id = max_id + 1;
    if (fresh->next_id < next) {
        fresh->next_id = next;
    }
    return 0;
}

uint32_t catalog_next_id(const catalog_t *catalog)
{
    return catalog == NULL ? 1 : catalog->next_id;
}

void catalog_set_next_id(catalog_t *catalog, uint32_t next_id)
{
    if (catalog != NULL && next_id > 0) {
        catalog->next_id = next_id;
    }
}

size_t catalog_count(const catalog_t *catalog)
{
    return catalog == NULL ? 0 : catalog->count;
}

const catalog_item_t *catalog_get(const catalog_t *catalog, size_t index)
{
    if (catalog == NULL || index >= catalog->count) {
        return NULL;
    }
    return &catalog->items[index];
}

const catalog_item_t *catalog_find_id(const catalog_t *catalog, uint32_t id)
{
    if (catalog == NULL || id == 0) {
        return NULL;
    }

    for (size_t i = 0; i < catalog->count; i++) {
        if (catalog->items[i].id == id) {
            return &catalog->items[i];
        }
    }

    return NULL;
}

size_t catalog_count_kind(const catalog_t *catalog, media_kind_t kind)
{
    size_t n = 0;

    if (catalog == NULL) {
        return 0;
    }

    for (size_t i = 0; i < catalog->count; i++) {
        if (catalog->items[i].kind == kind) {
            n++;
        }
    }

    return n;
}
