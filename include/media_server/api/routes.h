#ifndef MEDIA_SERVER_API_ROUTES_H
#define MEDIA_SERVER_API_ROUTES_H

#include "media_server/http/router.h"

/* Register built-in application routes. Returns 0 on success. */
int api_routes_register(router_t *router);

#endif /* MEDIA_SERVER_API_ROUTES_H */
