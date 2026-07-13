#ifndef MEDIA_SERVER_API_FAVOURITES_H
#define MEDIA_SERVER_API_FAVOURITES_H

#include "media_server/http/router.h"

void handle_favourites(const router_match_t *match, void *req, void *res);
void handle_favourite_put(const router_match_t *match, void *req, void *res);
void handle_favourite_delete(const router_match_t *match, void *req, void *res);

#endif /* MEDIA_SERVER_API_FAVOURITES_H */
