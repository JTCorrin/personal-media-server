#include "media_server/api/albums.h"

#include "media_server/api/browse_json.h"
#include "media_server/api/catalog_json.h"
#include "media_server/api/context.h"
#include "media_server/api/metadata_patch.h"
#include "media_server/api/page_json.h"
#include "media_server/api/params.h"
#include "media_server/http/json_body.h"
#include "media_server/http/server.h"
#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"
#include "media_server/library/runtime.h"
#include "media_server/media/file.h"
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
            if (append_catalog_item_json(&sb, item, index) != 0) {
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

void handle_album_patch(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    metadata_patch_t patch;
    uint32_t id;
    size_t updated = 0;
    string_buf_t sb;
    int rc;

    if (api_parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }
    if (api_parse_album_metadata_patch(req, &patch) != 0) {
        http_reply_json(res, 400, "{\"error\":\"invalid_body\"}");
        return;
    }
    rc = library_metadata_patch_album(ctx, id, &patch, &updated);
    if (rc == 1) {
        http_reply_json(res, 409, "{\"error\":\"library_busy\"}");
        return;
    }
    if (rc == 2) {
        http_reply_not_found(res);
        return;
    }
    if (rc != 0) {
        http_reply_json(res, 500, "{\"error\":\"update_failed\"}");
        return;
    }
    if (string_buf_init(&sb, 64) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }
    if (string_buf_append_fmt(&sb, "{\"updated_track_count\":%zu}", updated) != 0) {
        string_buf_free(&sb);
        http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
        return;
    }
    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
}

void handle_album_cover_put(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    uint32_t id = 0;
    char content_type[128];
    char ext[8];
    char rel_path[CATALOG_PATH_MAX];
    const char *body = NULL;
    size_t body_len = 0;
    string_buf_t sb;
    uint32_t cover_id = 0;
    int hdr_rc;
    int rc;

    if (api_parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }

    hdr_rc = http_header_get(req, "Content-Type", content_type, sizeof(content_type));
    if (hdr_rc != HTTP_QUERY_OK ||
        media_cover_ext_from_content_type(content_type, ext, sizeof(ext)) != 0) {
        http_reply_json(res, 400, "{\"error\":\"invalid_content_type\"}");
        return;
    }

    if (http_body_view(req, &body, &body_len) != 0 || body == NULL || body_len == 0) {
        http_reply_json(res, 400, "{\"error\":\"empty_body\"}");
        return;
    }
    if (body_len > LIBRARY_COVER_MAX_BYTES) {
        http_reply_json(res, 400, "{\"error\":\"body_too_large\"}");
        return;
    }

    rc = library_album_cover_put(ctx, id, body, body_len, ext, rel_path,
                                 sizeof(rel_path), &cover_id);
    if (rc == 1) {
        http_reply_json(res, 409, "{\"error\":\"library_busy\"}");
        return;
    }
    if (rc == 3) {
        http_reply_not_found(res);
        return;
    }
    if (rc == 4) {
        http_reply_json(res, 400, "{\"error\":\"no_library\"}");
        return;
    }
    if (rc == 5) {
        http_reply_json(res, 400, "{\"error\":\"no_album_dir\"}");
        return;
    }
    if (rc == 6) {
        http_reply_json(res, 400, "{\"error\":\"ambiguous_album_dir\"}");
        return;
    }
    if (rc == LIBRARY_COVER_WRITE_FAILED) {
        http_reply_json(res, 500, "{\"error\":\"write_failed\"}");
        return;
    }
    if (rc == LIBRARY_COVER_LINK_FAILED) {
        http_reply_json(res, 500, "{\"error\":\"cover_link_failed\"}");
        return;
    }
    if (rc == LIBRARY_COVER_CATALOG_SAVE_FAILED) {
        http_reply_json(res, 500, "{\"error\":\"catalog_save_failed\"}");
        return;
    }
    if (rc != 0) {
        http_reply_json(res, 500, "{\"error\":\"cover_update_failed\"}");
        return;
    }

    if (string_buf_init(&sb, 160) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }
    if (string_buf_append(&sb, "{\"ok\":true,\"path\":") != 0 ||
        string_buf_append_json_string(&sb, rel_path) != 0 ||
        string_buf_append_fmt(&sb, ",\"cover_id\":%u", cover_id) != 0 ||
        string_buf_append_char(&sb, '}') != 0) {
        string_buf_free(&sb);
        http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
        return;
    }
    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
}
