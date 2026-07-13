#ifndef MEDIA_SERVER_API_PLAYLISTS_H
#define MEDIA_SERVER_API_PLAYLISTS_H

#include "media_server/http/router.h"

void handle_playlists(const router_match_t *match, void *req, void *res);
void handle_playlist_create(const router_match_t *match, void *req, void *res);
void handle_playlist_by_id(const router_match_t *match, void *req, void *res);
void handle_playlist_patch(const router_match_t *match, void *req, void *res);
void handle_playlist_delete(const router_match_t *match, void *req, void *res);
void handle_playlist_tracks(const router_match_t *match, void *req, void *res);
void handle_playlist_tracks_put(const router_match_t *match, void *req, void *res);

#endif /* MEDIA_SERVER_API_PLAYLISTS_H */
