#include "media_server/api/routes.h"

#include "media_server/api/media_serve.h"
#include "media_server/api/ping.h"
#include "media_server/api/stub.h"
#include "media_server/api/tracks.h"
#include "media_server/util/log.h"

static int add_route(router_t *router, const char *method, const char *pattern,
                     router_handler_fn handler, void *user_data)
{
    if (router_add(router, method, pattern, handler, user_data) != 0) {
        LOG_ERROR("api", "failed to register %s %s", method, pattern);
        return -1;
    }
    return 0;
}

static int add_stub(router_t *router, const char *method, const char *pattern)
{
    return add_route(router, method, pattern, handle_not_implemented, NULL);
}

int api_routes_register(router_t *router, app_context_t *ctx)
{
    if (router == NULL) {
        return -1;
    }

    /* Live handlers */
    if (add_route(router, "GET", "/api/ping", handle_ping, NULL) != 0 ||
        add_route(router, "GET", "/api/tracks", handle_tracks, ctx) != 0 ||
        add_route(router, "GET", "/stream/:id", handle_stream, ctx) != 0 ||
        add_route(router, "GET", "/cover/:id", handle_cover, ctx) != 0) {
        return -1;
    }

    /* Browse / metadata stubs */
    if (add_stub(router, "GET", "/api/tracks/:id") != 0 ||
        add_stub(router, "GET", "/api/artists") != 0 ||
        add_stub(router, "GET", "/api/artists/:id") != 0 ||
        add_stub(router, "GET", "/api/artists/:id/albums") != 0 ||
        add_stub(router, "GET", "/api/albums") != 0 ||
        add_stub(router, "GET", "/api/albums/:id") != 0 ||
        add_stub(router, "GET", "/api/albums/:id/tracks") != 0 ||
        add_stub(router, "GET", "/api/images") != 0 ||
        add_stub(router, "GET", "/api/images/:id") != 0) {
        return -1;
    }

    /* Search / library stubs */
    if (add_stub(router, "GET", "/api/search") != 0 ||
        add_stub(router, "POST", "/api/library/scan") != 0 ||
        add_stub(router, "GET", "/api/library/status") != 0) {
        return -1;
    }

    /* Playlist stubs (persistence comes later) */
    if (add_stub(router, "GET", "/api/playlists") != 0 ||
        add_stub(router, "POST", "/api/playlists") != 0 ||
        add_stub(router, "GET", "/api/playlists/:id") != 0 ||
        add_stub(router, "PATCH", "/api/playlists/:id") != 0 ||
        add_stub(router, "DELETE", "/api/playlists/:id") != 0 ||
        add_stub(router, "GET", "/api/playlists/:id/tracks") != 0 ||
        add_stub(router, "PUT", "/api/playlists/:id/tracks") != 0) {
        return -1;
    }

    return 0;
}
