#include "media_server/api/images.h"

#include "media_server/api/catalog_json.h"
#include "media_server/api/context.h"
#include "media_server/media/kind.h"

void handle_images(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);

    api_context_lock(ctx);
    api_reply_catalog_kind_list(req, res, api_context_catalog_locked(ctx),
                                api_context_browse_locked(ctx),
                                MEDIA_KIND_IMAGE);
    api_context_unlock(ctx);
}

void handle_image_by_id(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);

    (void)req;
    api_context_lock(ctx);
    api_reply_catalog_kind_by_id(match, res, api_context_catalog_locked(ctx),
                                 api_context_browse_locked(ctx),
                                 MEDIA_KIND_IMAGE);
    api_context_unlock(ctx);
}
