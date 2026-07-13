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

catalog_t *catalog_clone(const catalog_t *src)
{
    catalog_t *copy;

    if (src == NULL) {
        return NULL;
    }
    copy = catalog_create();
    if (copy == NULL) {
        return NULL;
    }
    if (src->count > 0) {
        copy->items = malloc(src->count * sizeof(*copy->items));
        if (copy->items == NULL) {
            catalog_destroy(copy);
            return NULL;
        }
        memcpy(copy->items, src->items, src->count * sizeof(*copy->items));
        copy->count = src->count;
        copy->capacity = src->count;
    }
    copy->next_id = src->next_id;
    return copy;
}

int catalog_replace(catalog_t *dst, catalog_t *src)
{
    if (dst == NULL || src == NULL || dst == src) {
        return -1;
    }

    free(dst->items);
    dst->items = src->items;
    dst->count = src->count;
    dst->capacity = src->capacity;
    dst->next_id = src->next_id;

    src->items = NULL;
    src->count = 0;
    src->capacity = 0;
    src->next_id = 1;
    return 0;
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

int catalog_add_metadata(catalog_t *catalog, media_kind_t kind,
                         const char *rel_path, const media_metadata_t *metadata)
{
    catalog_item_t *item;
    const char *base;
    size_t path_len;

    if (catalog == NULL || rel_path == NULL || rel_path[0] == '\0' ||
        kind == MEDIA_KIND_NONE || catalog->next_id == UINT32_MAX) {
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
    if (metadata != NULL && kind == MEDIA_KIND_AUDIO) {
        item->scanned = *metadata;
        memcpy(item->artist, metadata->artist, sizeof(item->artist));
        memcpy(item->album, metadata->album, sizeof(item->album));
        memcpy(item->title, metadata->title, sizeof(item->title));
        memcpy(item->release_date, metadata->release_date,
               sizeof(item->release_date));
        memcpy(item->genre, metadata->genre, sizeof(item->genre));
        item->track_number = metadata->track_number;
        item->disc_number = metadata->disc_number;
    } else {
        memcpy(item->scanned.artist, item->artist, sizeof(item->artist));
        memcpy(item->scanned.album, item->album, sizeof(item->album));
        memcpy(item->scanned.title, item->title, sizeof(item->title));
    }

    catalog->count++;
    return 0;
}

int catalog_add(catalog_t *catalog, media_kind_t kind, const char *rel_path)
{
    return catalog_add_metadata(catalog, kind, rel_path, NULL);
}

int catalog_add_item(catalog_t *catalog, const catalog_item_t *item)
{
    catalog_item_t *dst;

    if (catalog == NULL || item == NULL || item->id == 0 ||
        (item->kind != MEDIA_KIND_AUDIO && item->kind != MEDIA_KIND_IMAGE) ||
        item->path[0] == '\0' || item->id == UINT32_MAX ||
        catalog_find_id(catalog, item->id) != NULL ||
        catalog_find_path(catalog, item->path) != NULL) {
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
            if (previous->items[i].id == UINT32_MAX) {
                return -1;
            }
            next = previous->items[i].id + 1;
        }
    }

    for (size_t i = 0; i < fresh->count; i++) {
        const catalog_item_t *old =
            catalog_find_path(previous, fresh->items[i].path);
        if (old != NULL) {
            fresh->items[i].id = old->id;
        } else {
            if (next == UINT32_MAX) {
                return -1;
            }
            fresh->items[i].id = next++;
        }
        if (fresh->items[i].id > max_id) {
            max_id = fresh->items[i].id;
        }
    }

    if (max_id == UINT32_MAX) {
        return -1;
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

static catalog_item_t *catalog_find_id_mut(catalog_t *catalog, uint32_t id)
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

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    size_t n = strlen(src);
    if (n >= dst_size) {
        n = dst_size - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void apply_patch_to_item(catalog_item_t *item, const metadata_patch_t *patch)
{
#define APPLY_TEXT(bit, field)                                                    \
    do {                                                                          \
        if ((patch->clear_mask & (bit)) != 0) {                                   \
            copy_text(item->field, sizeof(item->field), item->scanned.field);     \
            item->override_mask &= ~(bit);                                        \
        } else if ((patch->set_mask & (bit)) != 0) {                              \
            copy_text(item->field, sizeof(item->field), patch->values.field);     \
            item->override_mask |= (bit);                                         \
        }                                                                         \
    } while (0)

#define APPLY_UINT(bit, field)                                                    \
    do {                                                                          \
        if ((patch->clear_mask & (bit)) != 0) {                                   \
            item->field = item->scanned.field;                                    \
            item->override_mask &= ~(bit);                                        \
        } else if ((patch->set_mask & (bit)) != 0) {                              \
            item->field = patch->values.field;                                    \
            item->override_mask |= (bit);                                         \
        }                                                                         \
    } while (0)

    APPLY_TEXT(METADATA_FIELD_TITLE, title);
    APPLY_TEXT(METADATA_FIELD_ARTIST, artist);
    APPLY_TEXT(METADATA_FIELD_ALBUM, album);
    APPLY_TEXT(METADATA_FIELD_RELEASE_DATE, release_date);
    APPLY_TEXT(METADATA_FIELD_GENRE, genre);
    APPLY_UINT(METADATA_FIELD_TRACK_NUMBER, track_number);
    APPLY_UINT(METADATA_FIELD_DISC_NUMBER, disc_number);

#undef APPLY_TEXT
#undef APPLY_UINT
}

int catalog_apply_metadata_patch(catalog_t *catalog, uint32_t id,
                                 const metadata_patch_t *patch)
{
    catalog_item_t *item;

    if (catalog == NULL || patch == NULL ||
        ((patch->set_mask | patch->clear_mask) & ~METADATA_FIELD_ALL) != 0 ||
        (patch->set_mask & patch->clear_mask) != 0) {
        return -1;
    }
    item = catalog_find_id_mut(catalog, id);
    if (item == NULL || item->kind != MEDIA_KIND_AUDIO) {
        return -1;
    }
    apply_patch_to_item(item, patch);
    return 0;
}

int catalog_apply_metadata_override(catalog_t *catalog, const char *rel_path,
                                    const metadata_patch_t *override)
{
    const catalog_item_t *found;
    catalog_item_t *item;

    if (catalog == NULL || rel_path == NULL || override == NULL ||
        override->clear_mask != 0) {
        return -1;
    }
    found = catalog_find_path(catalog, rel_path);
    if (found == NULL || found->kind != MEDIA_KIND_AUDIO) {
        return 1;
    }
    item = catalog_find_id_mut(catalog, found->id);
    apply_patch_to_item(item, override);
    return 0;
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
