#include "media_server/api/routes.h"

#include "media_server/http/server.h"

#include "media_server/util/log.h"

static void handle_ping(const router_match_t *match, void *req, void *res)
{
    (void)match;
    (void)req;
    http_reply_json(res, 200, "{\"ok\":true}");
}

int api_routes_register(router_t *router)
{
    if (router == NULL) {
        return -1;
    }

    if (router_add(router, "GET", "/api/ping", handle_ping, NULL) != 0) {
        LOG_ERROR("api", "failed to register GET /api/ping");
        return -1;
    }

    return 0;
}
