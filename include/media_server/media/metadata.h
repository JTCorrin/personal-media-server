#ifndef MEDIA_SERVER_MEDIA_METADATA_H
#define MEDIA_SERVER_MEDIA_METADATA_H

#include "media_server/library/catalog.h"

/*
 * Derive path/filename fallback metadata. Common numbered filename forms are
 * reduced to their display title and track number.
 */
int media_metadata_from_path(const char *rel_path, media_metadata_t *out);

/*
 * Read embedded tags from abs_path over the path-derived fallback.
 * Missing/invalid tags are not errors; out remains populated from rel_path.
 */
int media_metadata_read(const char *abs_path, const char *rel_path,
                        media_metadata_t *out);

#endif /* MEDIA_SERVER_MEDIA_METADATA_H */
