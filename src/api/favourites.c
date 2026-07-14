#include "media_server/api/favourites.h"

#include "media_server/api/catalog_json.h"
#include "media_server/api/context.h"
#include "media_server/api/page_json.h"
#include "media_server/api/params.h"
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

void handle_favourites(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    user_store_t *store = require_user_store(match, res);
    api_page_t page;
    uint32_t ids[256];
    size_t total;
    size_t n;
    size_t take;
    size_t written = 0;
    string_buf_t sb;

    if (store == NULL) {
        return;
    }

    api_page_from_req(req, &page);
    total = user_store_favourite_count(store);
    take = page.limit < 256 ? page.limit : 256;
    n = user_store_favourite_list(store, page.offset, take, ids, 256);

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
            catalog_find_id(api_context_catalog_locked(ctx), ids[i]);
        if (item == NULL || item->kind != MEDIA_KIND_AUDIO) {
            continue;
        }
        if (written > 0 && string_buf_append_char(&sb, ',') != 0) {
            api_context_unlock(ctx);
            goto fail;
        }
        if (append_catalog_item_json(&sb, item,
                                     api_context_browse_locked(ctx)) != 0) {
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

void handle_favourite_put(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    user_store_t *store = require_user_store(match, res);
    uint32_t id = 0;
    const catalog_item_t *item;

    (void)req;
    if (store == NULL) {
        return;
    }
    if (api_parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }

    api_context_lock(ctx);
    item = catalog_find_id(api_context_catalog_locked(ctx), id);
    if (item == NULL || item->kind != MEDIA_KIND_AUDIO) {
        api_context_unlock(ctx);
        http_reply_not_found(res);
        return;
    }
    api_context_unlock(ctx);

    if (user_store_favourite_add(store, id) != 0) {
        http_reply_json(res, 500, "{\"error\":\"favourite_failed\"}");
        return;
    }
    http_reply_json(res, 200, "{\"ok\":true}");
}

void handle_favourite_delete(const router_match_t *match, void *req, void *res)
{
    user_store_t *store = require_user_store(match, res);
    uint32_t id = 0;

    (void)req;
    if (store == NULL) {
        return;
    }
    if (api_parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }
    if (user_store_favourite_remove(store, id) != 0) {
        http_reply_json(res, 500, "{\"error\":\"unfavourite_failed\"}");
        return;
    }
    http_reply_json(res, 200, "{\"ok\":true}");
}
