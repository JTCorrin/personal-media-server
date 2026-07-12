#ifndef MEDIA_SERVER_API_ROUTES_H
#define MEDIA_SERVER_API_ROUTES_H

#include "media_server/http/router.h"
#include "media_server/library/catalog.h"

typedef struct app_context {
    catalog_t *catalog;
    const char *library_dir; /* may be NULL if no library configured */
} app_context_t;

/* Register built-in application routes. ctx may be NULL (empty catalog). */
int api_routes_register(router_t *router, app_context_t *ctx);

#endif /* MEDIA_SERVER_API_ROUTES_H */
