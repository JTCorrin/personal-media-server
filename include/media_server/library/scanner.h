#ifndef MEDIA_SERVER_LIBRARY_SCANNER_H
#define MEDIA_SERVER_LIBRARY_SCANNER_H

#include "media_server/library/catalog.h"

/*
 * Recursively scan library_root and add recognized media files to catalog.
 * Paths stored in the catalog are relative to library_root.
 * Returns 0 on success, -1 on hard failure (e.g. root cannot be opened).
 */
int scanner_scan(const char *library_root, catalog_t *catalog);

#endif /* MEDIA_SERVER_LIBRARY_SCANNER_H */
