#ifndef MEDIA_SERVER_API_CONTEXT_H
#define MEDIA_SERVER_API_CONTEXT_H

#include "media_server/http/router.h"
#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"

#include <stddef.h>

/*
 * Application state passed as router user_data. All fields are borrowed:
 * they are owned by main() and must outlive the server.
 *
 * browse is built once from the catalog at startup (both are immutable while
 * the server runs). When the catalog moves into sqlite, browse queries become
 * SQL and this in-memory index goes away; until then, sharing one index avoids
 * rebuilding it on every /api/artists|albums|search request.
 */
typedef struct app_context {
    catalog_t *catalog;
    browse_index_t *browse;  /* may be NULL; treated as an empty index */
    const char *library_dir; /* may be NULL if no library configured */
} app_context_t;

/*
 * Accessors for handlers. All browse_* / catalog_* functions treat a NULL
 * index/catalog as empty, so handlers can use these results unconditionally.
 */
static inline app_context_t *api_context_from_match(const router_match_t *match)
{
    return match != NULL ? (app_context_t *)match->user_data : NULL;
}

static inline const browse_index_t *api_context_browse(const router_match_t *match)
{
    app_context_t *ctx = api_context_from_match(match);
    return ctx != NULL ? ctx->browse : NULL;
}

static inline catalog_t *api_context_catalog(const router_match_t *match)
{
    app_context_t *ctx = api_context_from_match(match);
    return ctx != NULL ? ctx->catalog : NULL;
}

#endif /* MEDIA_SERVER_API_CONTEXT_H */
