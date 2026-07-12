#ifndef MEDIA_SERVER_API_ALBUMS_H
#define MEDIA_SERVER_API_ALBUMS_H

#include "media_server/http/router.h"

void handle_albums(const router_match_t *match, void *req, void *res);
void handle_album_by_id(const router_match_t *match, void *req, void *res);
void handle_album_tracks(const router_match_t *match, void *req, void *res);

#endif /* MEDIA_SERVER_API_ALBUMS_H */
