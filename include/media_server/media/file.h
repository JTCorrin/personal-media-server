#ifndef MEDIA_SERVER_MEDIA_FILE_H
#define MEDIA_SERVER_MEDIA_FILE_H

#include <stddef.h>

/*
 * MIME type for a catalog item, derived from the path's extension.
 * Falls back to "application/octet-stream". Never returns NULL.
 */
const char *media_content_type(const char *path);

/*
 * Join library_root + rel_path into out.
 * Rejects NULL/empty, absolute rel_path, and any ".." segment.
 * Returns 0 on success, -1 on error.
 */
int media_resolve_path(const char *library_root, const char *rel_path, char *out,
                       size_t out_size);

#endif /* MEDIA_SERVER_MEDIA_FILE_H */
