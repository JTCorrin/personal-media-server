#ifndef MEDIA_SERVER_MEDIA_FILE_H
#define MEDIA_SERVER_MEDIA_FILE_H

#include <stddef.h>

/*
 * MIME type for a catalog item, derived from the path's extension.
 * Falls back to "application/octet-stream". Never returns NULL.
 */
const char *media_content_type(const char *path);

/*
 * Map an image Content-Type to a cover file extension without a leading dot
 * (jpg, png, or webp). Ignores optional ";..." parameters. Returns 0 on
 * success, -1 if unsupported or args invalid.
 */
int media_cover_ext_from_content_type(const char *content_type, char *ext_out,
                                      size_t ext_out_size);

/*
 * Join library_root + rel_path into out.
 * Rejects NULL/empty, absolute rel_path, and any ".." segment.
 * Returns 0 on success, -1 on error.
 */
int media_resolve_path(const char *library_root, const char *rel_path, char *out,
                       size_t out_size);

#endif /* MEDIA_SERVER_MEDIA_FILE_H */
