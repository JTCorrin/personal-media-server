#include "media_server/api/tracks.h"

#include "media_server/api/catalog_json.h"
#include "media_server/api/context.h"
#include "media_server/media/kind.h"

void handle_tracks(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = match->user_data;
    catalog_t *catalog = ctx != NULL ? ctx->catalog : NULL;

    api_reply_catalog_kind_list(req, res, catalog, MEDIA_KIND_AUDIO);
}

void handle_track_by_id(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = match->user_data;
    catalog_t *catalog = ctx != NULL ? ctx->catalog : NULL;

    (void)req;
    api_reply_catalog_kind_by_id(match, res, catalog, MEDIA_KIND_AUDIO);
}
