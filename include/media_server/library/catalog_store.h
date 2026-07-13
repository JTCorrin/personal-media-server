#ifndef MEDIA_SERVER_LIBRARY_CATALOG_STORE_H
#define MEDIA_SERVER_LIBRARY_CATALOG_STORE_H

#include "media_server/library/catalog.h"

/*
 * Snapshot persistence for catalog_t. Separate from --log-db.
 *
 * Load requires library_root to match the stored meta row.
 * Returns 0 on success, -1 on failure (missing file, root mismatch, corrupt).
 */
int catalog_store_load(const char *db_path, const char *library_root,
                       catalog_t *out);

/* Replace DB contents with catalog snapshot. Returns 0 on success. */
int catalog_store_save(const char *db_path, const char *library_root,
                       const catalog_t *catalog);

/* Merge all persisted path-keyed overrides into catalog effective metadata. */
int catalog_store_apply_overrides(const char *db_path, catalog_t *catalog);

/*
 * Apply one tri-state patch to each path in one SQLite transaction.
 * set_mask stores values; clear_mask removes those overrides.
 */
int catalog_store_patch_overrides(const char *db_path, const char *const *paths,
                                  size_t path_count,
                                  const metadata_patch_t *patch);

#endif /* MEDIA_SERVER_LIBRARY_CATALOG_STORE_H */
