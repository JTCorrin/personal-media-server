#include "media_server/api/images.h"

#include "media_server/api/catalog_json.h"
#include "media_server/api/context.h"
#include "media_server/media/kind.h"

void handle_images(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = match->user_data;
    catalog_t *catalog = ctx != NULL ? ctx->catalog : NULL;

    (void)req;
    api_reply_catalog_kind_list(res, catalog, MEDIA_KIND_IMAGE);
}

void handle_image_by_id(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = match->user_data;
    catalog_t *catalog = ctx != NULL ? ctx->catalog : NULL;

    (void)req;
    api_reply_catalog_kind_by_id(match, res, catalog, MEDIA_KIND_IMAGE);
}
