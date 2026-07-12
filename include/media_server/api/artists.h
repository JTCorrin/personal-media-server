#ifndef MEDIA_SERVER_API_ARTISTS_H
#define MEDIA_SERVER_API_ARTISTS_H

#include "media_server/http/router.h"

void handle_artists(const router_match_t *match, void *req, void *res);
void handle_artist_by_id(const router_match_t *match, void *req, void *res);
void handle_artist_albums(const router_match_t *match, void *req, void *res);

#endif /* MEDIA_SERVER_API_ARTISTS_H */
