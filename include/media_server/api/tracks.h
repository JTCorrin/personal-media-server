#ifndef MEDIA_SERVER_API_TRACKS_H
#define MEDIA_SERVER_API_TRACKS_H

#include "media_server/http/router.h"

void handle_tracks(const router_match_t *match, void *req, void *res);
void handle_track_by_id(const router_match_t *match, void *req, void *res);

#endif /* MEDIA_SERVER_API_TRACKS_H */
