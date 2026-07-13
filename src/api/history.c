#include "media_server/api/history.h"

#include "media_server/api/catalog_json.h"
#include "media_server/api/context.h"
#include "media_server/api/page_json.h"
#include "media_server/http/json_body.h"
#include "media_server/http/server.h"
#include "media_server/library/catalog.h"
#include "media_server/library/user_store.h"
#include "media_server/media/kind.h"
#include "media_server/util/string_buf.h"

static user_store_t *require_user_store(const router_match_t *match, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    if (ctx == NULL || ctx->user_store == NULL) {
        http_reply_json(res, 400, "{\"error\":\"no_user_db\"}");
        return NULL;
    }
    return ctx->user_store;
}

void handle_history(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    user_store_t *store = require_user_store(match, res);
    api_page_t page;
    user_history_entry_t entries[256];
    size_t total;
    size_t n;
    size_t take;
    size_t written = 0;
    string_buf_t sb;

    if (store == NULL) {
        return;
    }

    api_page_from_req(req, &page);
    total = user_store_history_count(store);
    take = page.limit < 256 ? page.limit : 256;
    n = user_store_history_list(store, page.offset, take, entries, 256);

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
            catalog_find_id(api_context_catalog_locked(ctx), entries[i].track_id);
        if (item == NULL || item->kind != MEDIA_KIND_AUDIO) {
            continue;
        }
        if (written > 0 && string_buf_append_char(&sb, ',') != 0) {
            api_context_unlock(ctx);
            goto fail;
        }
        if (string_buf_append_char(&sb, '{') != 0) {
            api_context_unlock(ctx);
            goto fail;
        }
        /* Embed track fields then played_unix — reuse catalog item as nested? */
        if (string_buf_append(&sb, "\"track\":") != 0 ||
            append_catalog_item_json(&sb, item) != 0 ||
            string_buf_append_fmt(&sb, ",\"played_unix\":%lld}",
                                  (long long)entries[i].played_unix) != 0) {
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

void handle_history_post(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    user_store_t *store = require_user_store(match, res);
    uint32_t track_id = 0;
    const catalog_item_t *item;

    if (store == NULL) {
        return;
    }
    if (http_req_json_get_u32(req, "track_id", &track_id) != 0 || track_id == 0) {
        http_reply_json(res, 400, "{\"error\":\"invalid_body\"}");
        return;
    }

    api_context_lock(ctx);
    item = catalog_find_id(api_context_catalog_locked(ctx), track_id);
    if (item == NULL || item->kind != MEDIA_KIND_AUDIO) {
        api_context_unlock(ctx);
        http_reply_json(res, 400, "{\"error\":\"invalid_track_id\"}");
        return;
    }
    api_context_unlock(ctx);

    if (user_store_history_append(store, track_id) != 0) {
        http_reply_json(res, 500, "{\"error\":\"history_failed\"}");
        return;
    }
    http_reply_json(res, 200, "{\"ok\":true}");
}
