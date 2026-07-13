#include "media_server/api/albums.h"

#include "media_server/api/browse_json.h"
#include "media_server/api/catalog_json.h"
#include "media_server/api/context.h"
#include "media_server/api/page_json.h"
#include "media_server/api/params.h"
#include "media_server/http/server.h"
#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"
#include "media_server/util/string_buf.h"

#include <stdint.h>

void handle_albums(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    const browse_index_t *index;
    api_page_t page;
    string_buf_t sb;
    size_t total = 0;
    size_t written = 0;
    size_t count;

    api_page_from_req(req, &page);

    api_context_lock(ctx);
    index = api_context_browse_locked(ctx);
    count = browse_album_count(index);

    if (string_buf_init(&sb, 64 + page.limit * 128) != 0) {
        api_context_unlock(ctx);
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    if (page_json_begin(&sb) != 0) {
        goto fail;
    }

    for (size_t i = 0; i < count; i++) {
        const browse_album_t *album = browse_album_get(index, i);
        if (album == NULL) {
            continue;
        }
        if (total >= page.offset && written < page.limit) {
            if (written > 0 && string_buf_append_char(&sb, ',') != 0) {
                goto fail;
            }
            if (append_album_json(&sb, album) != 0) {
                goto fail;
            }
            written++;
        }
        total++;
    }

    if (page_json_end(&sb, total, &page) != 0) {
        goto fail;
    }

    api_context_unlock(ctx);
    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
    return;

fail:
    api_context_unlock(ctx);
    string_buf_free(&sb);
    http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
}

void handle_album_by_id(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    const browse_index_t *index;
    uint32_t id = 0;
    const browse_album_t *album;
    string_buf_t sb;

    (void)req;

    if (api_parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }

    api_context_lock(ctx);
    index = api_context_browse_locked(ctx);
    album = browse_album_find_id(index, id);
    if (album == NULL) {
        api_context_unlock(ctx);
        http_reply_not_found(res);
        return;
    }

    if (string_buf_init(&sb, 160) != 0) {
        api_context_unlock(ctx);
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    if (append_album_json(&sb, album) != 0) {
        api_context_unlock(ctx);
        string_buf_free(&sb);
        http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
        return;
    }

    api_context_unlock(ctx);
    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
}

void handle_album_tracks(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    const browse_index_t *index;
    catalog_t *catalog;
    uint32_t id = 0;
    const browse_album_t *album;
    api_page_t page;
    string_buf_t sb;
    size_t total = 0;
    size_t written = 0;
    size_t count;

    if (api_parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }

    api_page_from_req(req, &page);

    api_context_lock(ctx);
    index = api_context_browse_locked(ctx);
    catalog = api_context_catalog_locked(ctx);
    album = browse_album_find_id(index, id);
    if (album == NULL) {
        api_context_unlock(ctx);
        http_reply_not_found(res);
        return;
    }

    count = catalog_count(catalog);

    if (string_buf_init(&sb, 64 + page.limit * 160) != 0) {
        api_context_unlock(ctx);
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    if (page_json_begin(&sb) != 0) {
        goto fail;
    }

    for (size_t i = 0; i < count; i++) {
        const catalog_item_t *item = catalog_get(catalog, i);
        if (item == NULL || !browse_album_owns_item(album, item)) {
            continue;
        }
        if (total >= page.offset && written < page.limit) {
            if (written > 0 && string_buf_append_char(&sb, ',') != 0) {
                goto fail;
            }
            if (append_catalog_item_json(&sb, item) != 0) {
                goto fail;
            }
            written++;
        }
        total++;
    }

    if (page_json_end(&sb, total, &page) != 0) {
        goto fail;
    }

    api_context_unlock(ctx);
    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
    return;

fail:
    api_context_unlock(ctx);
    string_buf_free(&sb);
    http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
}
