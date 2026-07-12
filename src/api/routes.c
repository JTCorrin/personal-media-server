#include "media_server/api/routes.h"

#include "media_server/http/server.h"
#include "media_server/media/kind.h"
#include "media_server/util/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void handle_ping(const router_match_t *match, void *req, void *res)
{
    (void)match;
    (void)req;
    http_reply_json(res, 200, "{\"ok\":true}");
}

static void handle_tracks(const router_match_t *match, void *req, void *res)
{
    catalog_t *catalog = match->user_data;
    size_t count = catalog_count(catalog);
    size_t cap = 64 + count * 256;
    char *json;
    size_t used = 0;

    (void)req;

    json = malloc(cap);
    if (json == NULL) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    used = (size_t)snprintf(json, cap, "[");
    for (size_t i = 0; i < count; i++) {
        const catalog_item_t *item = catalog_get(catalog, i);
        int n;

        if (item == NULL) {
            continue;
        }

        if (used + 200 >= cap) {
            size_t new_cap = cap * 2;
            char *grown = realloc(json, new_cap);
            if (grown == NULL) {
                free(json);
                http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
                return;
            }
            json = grown;
            cap = new_cap;
        }

        n = snprintf(json + used, cap - used,
                     "%s{\"id\":%u,\"kind\":\"%s\",\"path\":\"%s\",\"filename\":\"%s\"}",
                     i == 0 ? "" : ",", item->id, media_kind_name(item->kind),
                     item->path, item->filename);
        if (n < 0 || (size_t)n >= cap - used) {
            free(json);
            http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
            return;
        }
        used += (size_t)n;
    }

    if (used + 2 >= cap) {
        char *grown = realloc(json, used + 2);
        if (grown == NULL) {
            free(json);
            http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
            return;
        }
        json = grown;
    }
    json[used++] = ']';
    json[used] = '\0';

    http_reply_json(res, 200, json);
    free(json);
}

int api_routes_register(router_t *router, catalog_t *catalog)
{
    if (router == NULL) {
        return -1;
    }

    if (router_add(router, "GET", "/api/ping", handle_ping, NULL) != 0) {
        LOG_ERROR("api", "failed to register GET /api/ping");
        return -1;
    }

    if (router_add(router, "GET", "/api/tracks", handle_tracks, catalog) != 0) {
        LOG_ERROR("api", "failed to register GET /api/tracks");
        return -1;
    }

    return 0;
}
