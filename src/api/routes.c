#include "media_server/api/routes.h"

#include "media_server/http/server.h"
#include "media_server/media/file.h"
#include "media_server/media/kind.h"
#include "media_server/util/log.h"
#include "media_server/util/string_buf.h"

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

/* Append one catalog item as a JSON object. Returns 0 on success. */
static int append_track_json(string_buf_t *sb, const catalog_item_t *item)
{
    if (string_buf_append_fmt(sb, "{\"id\":%u,\"kind\":\"%s\",\"path\":", item->id,
                              media_kind_name(item->kind)) != 0) {
        return -1;
    }
    if (string_buf_append_json_string(sb, item->path) != 0) {
        return -1;
    }
    if (string_buf_append(sb, ",\"filename\":") != 0) {
        return -1;
    }
    if (string_buf_append_json_string(sb, item->filename) != 0) {
        return -1;
    }
    return string_buf_append_char(sb, '}');
}

static void handle_tracks(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = match->user_data;
    catalog_t *catalog = ctx != NULL ? ctx->catalog : NULL;
    size_t count = catalog_count(catalog);
    string_buf_t sb;

    (void)req;

    if (string_buf_init(&sb, 64 + count * 128) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    if (string_buf_append_char(&sb, '[') != 0) {
        goto fail;
    }

    for (size_t i = 0; i < count; i++) {
        const catalog_item_t *item = catalog_get(catalog, i);

        if (item == NULL) {
            continue;
        }

        if (i > 0 && string_buf_append_char(&sb, ',') != 0) {
            goto fail;
        }
        if (append_track_json(&sb, item) != 0) {
            goto fail;
        }
    }

    if (string_buf_append_char(&sb, ']') != 0) {
        goto fail;
    }

    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
    return;

fail:
    string_buf_free(&sb);
    http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
}

static int parse_id_param(const router_match_t *match, uint32_t *out_id)
{
    const char *raw;
    char *end = NULL;
    unsigned long long value;

    if (match == NULL || out_id == NULL) {
        return -1;
    }

    raw = router_param_get(match, "id");
    if (raw == NULL || raw[0] == '\0') {
        return -1;
    }

    /*
     * strtoull, not strtoul: unsigned long may be 32-bit, which would make
     * the range check below a no-op on such platforms.
     */
    errno = 0;
    value = strtoull(raw, &end, 10);
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

    ctype = media_content_type(item->path);
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
