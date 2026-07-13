#include "media_server/api/routes.h"

#include "media_server/api/albums.h"
#include "media_server/api/artists.h"
#include "media_server/api/discover.h"
#include "media_server/api/favourites.h"
#include "media_server/api/history.h"
#include "media_server/api/images.h"
#include "media_server/api/library.h"
#include "media_server/api/media_serve.h"
#include "media_server/api/ping.h"
#include "media_server/api/playlists.h"
#include "media_server/api/search.h"
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

int api_routes_register(router_t *router, app_context_t *ctx)
{
    if (router == NULL) {
        return -1;
    }

    if (add_route(router, "GET", "/api/ping", handle_ping, NULL) != 0 ||
        add_route(router, "GET", "/api/tracks", handle_tracks, ctx) != 0 ||
        add_route(router, "GET", "/api/tracks/:id", handle_track_by_id, ctx) != 0 ||
        add_route(router, "GET", "/api/images", handle_images, ctx) != 0 ||
        add_route(router, "GET", "/api/images/:id", handle_image_by_id, ctx) != 0 ||
        add_route(router, "GET", "/api/artists", handle_artists, ctx) != 0 ||
        add_route(router, "GET", "/api/artists/:id", handle_artist_by_id, ctx) != 0 ||
        add_route(router, "GET", "/api/artists/:id/albums", handle_artist_albums, ctx) !=
            0 ||
        add_route(router, "GET", "/api/albums", handle_albums, ctx) != 0 ||
        add_route(router, "GET", "/api/albums/:id", handle_album_by_id, ctx) != 0 ||
        add_route(router, "GET", "/api/albums/:id/tracks", handle_album_tracks, ctx) !=
            0 ||
        add_route(router, "GET", "/api/search", handle_search, ctx) != 0 ||
        add_route(router, "GET", "/api/library/status", handle_library_status, ctx) !=
            0 ||
        add_route(router, "POST", "/api/library/scan", handle_library_scan, ctx) != 0 ||
        add_route(router, "GET", "/api/playlists", handle_playlists, ctx) != 0 ||
        add_route(router, "POST", "/api/playlists", handle_playlist_create, ctx) != 0 ||
        add_route(router, "GET", "/api/playlists/:id", handle_playlist_by_id, ctx) != 0 ||
        add_route(router, "PATCH", "/api/playlists/:id", handle_playlist_patch, ctx) !=
            0 ||
        add_route(router, "DELETE", "/api/playlists/:id", handle_playlist_delete, ctx) !=
            0 ||
        add_route(router, "GET", "/api/playlists/:id/tracks", handle_playlist_tracks,
                  ctx) != 0 ||
        add_route(router, "PUT", "/api/playlists/:id/tracks", handle_playlist_tracks_put,
                  ctx) != 0 ||
        add_route(router, "GET", "/api/favourites", handle_favourites, ctx) != 0 ||
        add_route(router, "PUT", "/api/favourites/:id", handle_favourite_put, ctx) != 0 ||
        add_route(router, "DELETE", "/api/favourites/:id", handle_favourite_delete,
                  ctx) != 0 ||
        add_route(router, "GET", "/api/history", handle_history, ctx) != 0 ||
        add_route(router, "POST", "/api/history", handle_history_post, ctx) != 0 ||
        add_route(router, "GET", "/api/discover/random", handle_discover_random, ctx) !=
            0 ||
        add_route(router, "GET", "/api/discover/recent", handle_discover_recent, ctx) !=
            0 ||
        add_route(router, "GET", "/api/discover/recently-played",
                  handle_discover_recently_played, ctx) != 0 ||
        add_route(router, "GET", "/stream/:id", handle_stream, ctx) != 0 ||
        add_route(router, "GET", "/cover/:id", handle_cover, ctx) != 0) {
        return -1;
    }

    return 0;
}
