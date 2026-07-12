#ifndef MEDIA_SERVER_API_MEDIA_SERVE_H
#define MEDIA_SERVER_API_MEDIA_SERVE_H

#include "media_server/http/router.h"

void handle_stream(const router_match_t *match, void *req, void *res);
void handle_cover(const router_match_t *match, void *req, void *res);

#endif /* MEDIA_SERVER_API_MEDIA_SERVE_H */
