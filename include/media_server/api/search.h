#ifndef MEDIA_SERVER_API_SEARCH_H
#define MEDIA_SERVER_API_SEARCH_H

#include "media_server/http/router.h"

void handle_search(const router_match_t *match, void *req, void *res);

#endif /* MEDIA_SERVER_API_SEARCH_H */
