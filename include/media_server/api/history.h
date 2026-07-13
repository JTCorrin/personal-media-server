#ifndef MEDIA_SERVER_API_HISTORY_H
#define MEDIA_SERVER_API_HISTORY_H

#include "media_server/http/router.h"

void handle_history(const router_match_t *match, void *req, void *res);
void handle_history_post(const router_match_t *match, void *req, void *res);

#endif /* MEDIA_SERVER_API_HISTORY_H */
