#ifndef MEDIA_SERVER_API_ROUTES_H
#define MEDIA_SERVER_API_ROUTES_H

#include "media_server/http/router.h"
#include "media_server/library/catalog.h"

/* Register built-in application routes. catalog may be NULL (tracks empty). */
int api_routes_register(router_t *router, catalog_t *catalog);

#endif /* MEDIA_SERVER_API_ROUTES_H */
