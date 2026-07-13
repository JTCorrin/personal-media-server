#ifndef MEDIA_SERVER_API_DISCOVER_H
#define MEDIA_SERVER_API_DISCOVER_H

#include "media_server/http/router.h"

void handle_discover_random(const router_match_t *match, void *req, void *res);
void handle_discover_recent(const router_match_t *match, void *req, void *res);
void handle_discover_recently_played(const router_match_t *match, void *req,
                                     void *res);

#endif /* MEDIA_SERVER_API_DISCOVER_H */
