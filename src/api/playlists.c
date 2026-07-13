#include "media_server/api/playlists.h"

#include "media_server/api/catalog_json.h"
#include "media_server/api/context.h"
#include "media_server/api/page_json.h"
#include "media_server/api/params.h"
#include "media_server/http/json_body.h"
#include "media_server/http/server.h"
#include "media_server/library/catalog.h"
#include "media_server/library/user_store.h"
#include "media_server/media/kind.h"
#include "media_server/util/string_buf.h"

#include <stdint.h>

#define PLAYLIST_TRACKS_MAX 4096

static user_store_t *require_user_store(const router_match_t *match, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    if (ctx == NULL || ctx->user_store == NULL) {
        http_reply_json(res, 400, "{\"error\":\"no_user_db\"}");
        return NULL;
    }
    return ctx->user_store;
}

static int append_playlist_json(string_buf_t *sb, const user_playlist_t *pl)
{
    if (string_buf_append_fmt(sb, "{\"id\":%u,\"name\":", pl->id) != 0) {
        return -1;
    }
    if (string_buf_append_json_string(sb, pl->name) != 0) {
        return -1;
    }
    return string_buf_append_fmt(
        sb, ",\"track_count\":%zu,\"created_unix\":%lld,\"updated_unix\":%lld}",
        pl->track_count, (long long)pl->created_unix, (long long)pl->updated_unix);
}

void handle_playlists(const router_match_t *match, void *req, void *res)
{
    user_store_t *store = require_user_store(match, res);
    api_page_t page;
    user_playlist_t items[64];
    size_t total;
    size_t n;
    size_t take;
    string_buf_t sb;

    if (store == NULL) {
        return;
    }

    api_page_from_req(req, &page);
    total = user_store_playlist_count(store);
    take = page.limit < 64 ? page.limit : 64;
    n = user_store_playlist_list(store, page.offset, take, items, 64);

    if (string_buf_init(&sb, 256) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }
    if (page_json_begin(&sb) != 0) {
        goto fail;
    }
    for (size_t i = 0; i < n; i++) {
        if (i > 0 && string_buf_append_char(&sb, ',') != 0) {
            goto fail;
        }
        if (append_playlist_json(&sb, &items[i]) != 0) {
            goto fail;
        }
    }
    if (page_json_end(&sb, total, &page) != 0) {
        goto fail;
    }
    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
    return;

fail:
    string_buf_free(&sb);
    http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
}

void handle_playlist_create(const router_match_t *match, void *req, void *res)
{
    user_store_t *store = require_user_store(match, res);
    char name[USER_STORE_NAME_MAX];
    user_playlist_t pl;
    string_buf_t sb;

    if (store == NULL) {
        return;
    }

    if (http_req_json_get_string(req, "name", name, sizeof(name)) != 0 ||
        name[0] == '\0') {
        http_reply_json(res, 400, "{\"error\":\"invalid_body\"}");
        return;
    }

    if (user_store_playlist_create(store, name, &pl) != 0) {
        http_reply_json(res, 500, "{\"error\":\"create_failed\"}");
        return;
    }

    if (string_buf_init(&sb, 128) != 0 || append_playlist_json(&sb, &pl) != 0) {
        string_buf_free(&sb);
        http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
        return;
    }
    http_reply_json(res, 201, string_buf_cstr(&sb));
    string_buf_free(&sb);
}

void handle_playlist_by_id(const router_match_t *match, void *req, void *res)
{
    user_store_t *store = require_user_store(match, res);
    uint32_t id = 0;
    user_playlist_t pl;
    string_buf_t sb;

    (void)req;
    if (store == NULL) {
        return;
    }
    if (api_parse_id_param(match, &id) != 0 ||
        user_store_playlist_get(store, id, &pl) != 0) {
        http_reply_not_found(res);
        return;
    }
    if (string_buf_init(&sb, 128) != 0 || append_playlist_json(&sb, &pl) != 0) {
        string_buf_free(&sb);
        http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
        return;
    }
    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
}

void handle_playlist_patch(const router_match_t *match, void *req, void *res)
{
    user_store_t *store = require_user_store(match, res);
    uint32_t id = 0;
    char name[USER_STORE_NAME_MAX];
    user_playlist_t pl;
    string_buf_t sb;

    if (store == NULL) {
        return;
    }
    if (api_parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }
    if (http_req_json_get_string(req, "name", name, sizeof(name)) != 0 ||
        name[0] == '\0') {
        http_reply_json(res, 400, "{\"error\":\"invalid_body\"}");
        return;
    }
    if (user_store_playlist_rename(store, id, name) != 0) {
        http_reply_not_found(res);
        return;
    }
    if (user_store_playlist_get(store, id, &pl) != 0) {
        http_reply_not_found(res);
        return;
    }
    if (string_buf_init(&sb, 128) != 0 || append_playlist_json(&sb, &pl) != 0) {
        string_buf_free(&sb);
        http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
        return;
    }
    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
}

void handle_playlist_delete(const router_match_t *match, void *req, void *res)
{
    user_store_t *store = require_user_store(match, res);
    uint32_t id = 0;

    (void)req;
    if (store == NULL) {
        return;
    }
    if (api_parse_id_param(match, &id) != 0 ||
        user_store_playlist_delete(store, id) != 0) {
        http_reply_not_found(res);
        return;
    }
    http_reply_json(res, 200, "{\"ok\":true}");
}

void handle_playlist_tracks(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    user_store_t *store = require_user_store(match, res);
    uint32_t id = 0;
    api_page_t page;
    uint32_t track_ids[256];
    size_t total;
    size_t n;
    size_t take;
    size_t written = 0;
    string_buf_t sb;

    if (store == NULL) {
        return;
    }
    if (api_parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }
    {
        user_playlist_t pl;
        if (user_store_playlist_get(store, id, &pl) != 0) {
            http_reply_not_found(res);
            return;
        }
    }

    api_page_from_req(req, &page);
    total = user_store_playlist_track_count(store, id);
    take = page.limit < 256 ? page.limit : 256;
    n = user_store_playlist_get_tracks(store, id, page.offset, take, track_ids, 256);

    if (string_buf_init(&sb, 256) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }
    if (page_json_begin(&sb) != 0) {
        goto fail;
    }

    api_context_lock(ctx);
    for (size_t i = 0; i < n; i++) {
        const catalog_item_t *item =
            catalog_find_id(api_context_catalog_locked(ctx), track_ids[i]);
        if (item == NULL || item->kind != MEDIA_KIND_AUDIO) {
            continue;
        }
        if (written > 0 && string_buf_append_char(&sb, ',') != 0) {
            api_context_unlock(ctx);
            goto fail;
        }
        if (append_catalog_item_json(&sb, item) != 0) {
            api_context_unlock(ctx);
            goto fail;
        }
        written++;
    }
    api_context_unlock(ctx);

    if (page_json_end(&sb, total, &page) != 0) {
        goto fail;
    }
    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
    return;

fail:
    string_buf_free(&sb);
    http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
}

void handle_playlist_tracks_put(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    user_store_t *store = require_user_store(match, res);
    uint32_t id = 0;
    uint32_t track_ids[PLAYLIST_TRACKS_MAX];
    size_t count = 0;

    if (store == NULL) {
        return;
    }
    if (api_parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }
    {
        user_playlist_t pl;
        if (user_store_playlist_get(store, id, &pl) != 0) {
            http_reply_not_found(res);
            return;
        }
    }

    if (http_req_json_get_u32_array(req, "track_ids", track_ids, PLAYLIST_TRACKS_MAX,
                                    &count) != 0) {
        http_reply_json(res, 400, "{\"error\":\"invalid_body\"}");
        return;
    }

    api_context_lock(ctx);
    for (size_t i = 0; i < count; i++) {
        const catalog_item_t *item =
            catalog_find_id(api_context_catalog_locked(ctx), track_ids[i]);
        if (item == NULL || item->kind != MEDIA_KIND_AUDIO) {
            api_context_unlock(ctx);
            http_reply_json(res, 400, "{\"error\":\"invalid_track_id\"}");
            return;
        }
    }
    api_context_unlock(ctx);

    if (user_store_playlist_set_tracks(store, id, track_ids, count) != 0) {
        http_reply_json(res, 500, "{\"error\":\"update_failed\"}");
        return;
    }
    http_reply_json(res, 200, "{\"ok\":true}");
}
