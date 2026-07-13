#include "media_server/api/tracks.h"

#include "media_server/api/catalog_json.h"
#include "media_server/api/context.h"
#include "media_server/api/metadata_patch.h"
#include "media_server/api/params.h"
#include "media_server/http/server.h"
#include "media_server/library/runtime.h"
#include "media_server/media/kind.h"
#include "media_server/util/string_buf.h"

void handle_tracks(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);

    api_context_lock(ctx);
    api_reply_catalog_kind_list(req, res, api_context_catalog_locked(ctx),
                                MEDIA_KIND_AUDIO);
    api_context_unlock(ctx);
}

void handle_track_by_id(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);

    (void)req;
    api_context_lock(ctx);
    api_reply_catalog_kind_by_id(match, res, api_context_catalog_locked(ctx),
                                 MEDIA_KIND_AUDIO);
    api_context_unlock(ctx);
}

void handle_track_patch(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    metadata_patch_t patch;
    catalog_item_t updated;
    string_buf_t sb;
    uint32_t id;
    int rc;

    if (api_parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }
    if (api_parse_track_metadata_patch(req, &patch) != 0) {
        http_reply_json(res, 400, "{\"error\":\"invalid_body\"}");
        return;
    }
    rc = library_metadata_patch_track(ctx, id, &patch, &updated);
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
    if (string_buf_init(&sb, 512) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }
    if (append_catalog_item_json(&sb, &updated) != 0) {
        string_buf_free(&sb);
        http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
        return;
    }
    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
}
