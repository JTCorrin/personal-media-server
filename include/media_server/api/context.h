#ifndef MEDIA_SERVER_API_CONTEXT_H
#define MEDIA_SERVER_API_CONTEXT_H

#include "media_server/http/router.h"
#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"
#include "media_server/library/user_store.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/*
 * Application state passed as router user_data. Owned by main().
 *
 * catalog/browse may be replaced by a background rescan; take
 * api_context_lock() while reading them. library_dir is immutable.
 */
typedef struct app_context {
    catalog_t *catalog;
    browse_index_t *browse;  /* may be NULL; treated as an empty index */
    const char *library_dir; /* may be NULL if no library configured */
    const char *catalog_db_path; /* may be NULL; snapshot path for rescans */
    const char *user_db_path;    /* may be NULL */
    user_store_t *user_store;    /* NULL if no user_db */

    pthread_mutex_t mu;
    bool scanning;
    bool worker_joinable;
    pthread_t worker;
    atomic_bool cancel_scan;
    time_t last_scan_unix;
    bool last_scan_ok;
    char last_error[128];
} app_context_t;

static inline app_context_t *api_context_from_match(const router_match_t *match)
{
    return match != NULL ? (app_context_t *)match->user_data : NULL;
}

static inline void api_context_lock(app_context_t *ctx)
{
    if (ctx != NULL) {
        pthread_mutex_lock(&ctx->mu);
    }
}

static inline void api_context_unlock(app_context_t *ctx)
{
    if (ctx != NULL) {
        pthread_mutex_unlock(&ctx->mu);
    }
}

/* Call only while holding api_context_lock(). */
static inline const browse_index_t *api_context_browse_locked(const app_context_t *ctx)
{
    return ctx != NULL ? ctx->browse : NULL;
}

static inline catalog_t *api_context_catalog_locked(const app_context_t *ctx)
{
    return ctx != NULL ? ctx->catalog : NULL;
}

#endif /* MEDIA_SERVER_API_CONTEXT_H */
