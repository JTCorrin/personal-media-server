#ifndef MEDIA_SERVER_API_BROWSE_JSON_H
#define MEDIA_SERVER_API_BROWSE_JSON_H

#include "media_server/library/browse.h"
#include "media_server/util/string_buf.h"

int append_artist_json(string_buf_t *sb, const browse_artist_t *artist);
int append_album_json(string_buf_t *sb, const browse_album_t *album);

#endif /* MEDIA_SERVER_API_BROWSE_JSON_H */
