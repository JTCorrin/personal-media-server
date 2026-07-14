#ifndef MEDIA_SERVER_API_CATALOG_JSON_H
#define MEDIA_SERVER_API_CATALOG_JSON_H

#include "media_server/http/router.h"
#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"
#include "media_server/media/kind.h"
#include "media_server/util/string_buf.h"

/* Append one catalog item as a JSON object. Returns 0 on success. */
int append_catalog_item_json(string_buf_t *sb, const catalog_item_t *item,
                             const browse_index_t *browse);

/* Paginated JSON list of items matching kind (reads limit/offset from req). */
void api_reply_catalog_kind_list(void *req, void *res, catalog_t *catalog,
                                 const browse_index_t *browse, media_kind_t kind);

/* Single JSON object for :id if it matches kind; else 404. */
void api_reply_catalog_kind_by_id(const router_match_t *match, void *res,
                                  catalog_t *catalog,
                                  const browse_index_t *browse,
                                  media_kind_t kind);

#endif /* MEDIA_SERVER_API_CATALOG_JSON_H */
