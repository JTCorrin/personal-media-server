#ifndef MEDIA_SERVER_API_METADATA_PATCH_H
#define MEDIA_SERVER_API_METADATA_PATCH_H

#include "media_server/library/catalog.h"

/* Returns 0 for a valid non-empty patch, -1 otherwise. */
int api_parse_track_metadata_patch(void *req, metadata_patch_t *out);
int api_parse_album_metadata_patch(void *req, metadata_patch_t *out);

#endif /* MEDIA_SERVER_API_METADATA_PATCH_H */
