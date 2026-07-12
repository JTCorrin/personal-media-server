#include "media_server/api/routes.h"

#include "media_server/http/server.h"
#include "media_server/media/file.h"
#include "media_server/media/kind.h"
#include "media_server/util/log.h"

#include <errno.h>
#include <stdint.h>
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
    app_context_t *ctx = match->user_data;
    catalog_t *catalog = ctx != NULL ? ctx->catalog : NULL;
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

static int parse_id_param(const router_match_t *match, uint32_t *out_id)
{
    const char *raw;
    char *end = NULL;
    unsigned long value;

    if (match == NULL || out_id == NULL) {
        return -1;
    }

    raw = router_param_get(match, "id");
    if (raw == NULL || raw[0] == '\0') {
        return -1;
    }

    errno = 0;
    value = strtoul(raw, &end, 10);
    if (errno != 0 || end == raw || *end != '\0' || value == 0 || value > UINT32_MAX) {
        return -1;
    }

    *out_id = (uint32_t)value;
    return 0;
}

static void serve_catalog_file(const router_match_t *match, void *req, void *res,
                               media_kind_t required_kind)
{
    app_context_t *ctx = match->user_data;
    uint32_t id = 0;
    const catalog_item_t *item;
    char abs_path[CATALOG_PATH_MAX * 2];
    const char *ctype;

    if (ctx == NULL || ctx->catalog == NULL || ctx->library_dir == NULL ||
        ctx->library_dir[0] == '\0') {
        http_reply_not_found(res);
        return;
    }

    if (parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }

    item = catalog_find_id(ctx->catalog, id);
    if (item == NULL || item->kind != required_kind) {
        http_reply_not_found(res);
        return;
    }

    if (media_resolve_path(ctx->library_dir, item->path, abs_path, sizeof(abs_path)) !=
        0) {
        http_reply_not_found(res);
        return;
    }

    ctype = media_content_type(item->kind, item->path);
    http_reply_file(req, res, abs_path, ctype);
}

static void handle_stream(const router_match_t *match, void *req, void *res)
{
    serve_catalog_file(match, req, res, MEDIA_KIND_AUDIO);
}

static void handle_cover(const router_match_t *match, void *req, void *res)
{
    serve_catalog_file(match, req, res, MEDIA_KIND_IMAGE);
}

int api_routes_register(router_t *router, app_context_t *ctx)
{
    if (router == NULL) {
        return -1;
    }

    if (router_add(router, "GET", "/api/ping", handle_ping, NULL) != 0) {
        LOG_ERROR("api", "failed to register GET /api/ping");
        return -1;
    }

    if (router_add(router, "GET", "/api/tracks", handle_tracks, ctx) != 0) {
        LOG_ERROR("api", "failed to register GET /api/tracks");
        return -1;
    }

    if (router_add(router, "GET", "/stream/:id", handle_stream, ctx) != 0) {
        LOG_ERROR("api", "failed to register GET /stream/:id");
        return -1;
    }

    if (router_add(router, "GET", "/cover/:id", handle_cover, ctx) != 0) {
        LOG_ERROR("api", "failed to register GET /cover/:id");
        return -1;
    }

    return 0;
}
