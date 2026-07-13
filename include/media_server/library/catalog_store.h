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

#endif /* MEDIA_SERVER_LIBRARY_CATALOG_STORE_H */
