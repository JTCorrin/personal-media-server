#ifndef MEDIA_SERVER_MEDIA_FILE_H
#define MEDIA_SERVER_MEDIA_FILE_H

#include "media_server/media/kind.h"

#include <stddef.h>

/*
 * MIME type for a catalog item. Uses extension when helpful; falls back by kind.
 * Never returns NULL.
 */
const char *media_content_type(media_kind_t kind, const char *path);

/*
 * Join library_root + rel_path into out.
 * Rejects NULL/empty, absolute rel_path, and any ".." segment.
 * Returns 0 on success, -1 on error.
 */
int media_resolve_path(const char *library_root, const char *rel_path, char *out,
                       size_t out_size);

#endif /* MEDIA_SERVER_MEDIA_FILE_H */
