#include "media_server/api/media_serve.h"

#include "media_server/api/context.h"
#include "media_server/api/params.h"
#include "media_server/http/server.h"
#include "media_server/library/catalog.h"
#include "media_server/media/file.h"
#include "media_server/media/kind.h"

#include <stdint.h>
#include <string.h>

static void serve_catalog_file(const router_match_t *match, void *req, void *res,
                               media_kind_t required_kind)
{
    app_context_t *ctx = api_context_from_match(match);
    uint32_t id = 0;
    const catalog_item_t *item;
    char abs_path[CATALOG_PATH_MAX * 2];
    char rel_path[CATALOG_PATH_MAX];
    const char *ctype;
    const char *library_dir;

    if (api_parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }

    api_context_lock(ctx);
    if (ctx == NULL || ctx->catalog == NULL || ctx->library_dir == NULL ||
        ctx->library_dir[0] == '\0') {
        api_context_unlock(ctx);
        http_reply_not_found(res);
        return;
    }

    item = catalog_find_id(ctx->catalog, id);
    if (item == NULL || item->kind != required_kind) {
        api_context_unlock(ctx);
        http_reply_not_found(res);
        return;
    }

    memcpy(rel_path, item->path, sizeof(rel_path));
    library_dir = ctx->library_dir;
    api_context_unlock(ctx);

    if (media_resolve_path(library_dir, rel_path, abs_path, sizeof(abs_path)) != 0) {
        http_reply_not_found(res);
        return;
    }

    ctype = media_content_type(rel_path);
    http_reply_file(req, res, abs_path, ctype);
}

void handle_stream(const router_match_t *match, void *req, void *res)
{
    serve_catalog_file(match, req, res, MEDIA_KIND_AUDIO);
}

void handle_cover(const router_match_t *match, void *req, void *res)
{
    serve_catalog_file(match, req, res, MEDIA_KIND_IMAGE);
}
