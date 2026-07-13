#include "media_server/library/runtime.h"

#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"
#include "media_server/library/catalog_store.h"
#include "media_server/library/scanner.h"
#include "media_server/media/kind.h"
#include "media_server/util/log.h"

#include <string.h>
#include <time.h>

int library_runtime_init(app_context_t *ctx)
{
    if (ctx == NULL) {
        return -1;
    }

    if (pthread_mutex_init(&ctx->mu, NULL) != 0) {
        return -1;
    }

    ctx->scanning = false;
    ctx->worker_alive = false;
    ctx->cancel_scan = 0;
    ctx->last_scan_unix = 0;
    ctx->last_scan_ok = false;
    ctx->last_error[0] = '\0';
    return 0;
}

void library_runtime_shutdown(app_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    pthread_mutex_lock(&ctx->mu);
    if (ctx->worker_alive) {
        ctx->cancel_scan = 1;
        pthread_mutex_unlock(&ctx->mu);
        pthread_join(ctx->worker, NULL);
        pthread_mutex_lock(&ctx->mu);
        ctx->worker_alive = false;
        ctx->scanning = false;
    }
    pthread_mutex_unlock(&ctx->mu);
    pthread_mutex_destroy(&ctx->mu);
}

static void set_error(app_context_t *ctx, const char *msg)
{
    if (msg == NULL) {
        ctx->last_error[0] = '\0';
        return;
    }
    strncpy(ctx->last_error, msg, sizeof(ctx->last_error) - 1);
    ctx->last_error[sizeof(ctx->last_error) - 1] = '\0';
}

static void *scan_worker(void *arg)
{
    app_context_t *ctx = arg;
    catalog_t *new_catalog = NULL;
    browse_index_t *new_browse = NULL;
    catalog_t *old_catalog = NULL;
    browse_index_t *old_browse = NULL;
    const char *library_dir;
    const char *catalog_db_path;

    pthread_mutex_lock(&ctx->mu);
    library_dir = ctx->library_dir;
    catalog_db_path = ctx->catalog_db_path;
    old_catalog = ctx->catalog;
    pthread_mutex_unlock(&ctx->mu);

    new_catalog = catalog_create();
    if (new_catalog == NULL) {
        pthread_mutex_lock(&ctx->mu);
        ctx->scanning = false;
        ctx->worker_alive = false;
        ctx->last_scan_ok = false;
        set_error(ctx, "out of memory");
        pthread_mutex_unlock(&ctx->mu);
        return NULL;
    }

    LOG_INFO("library", "background scan started: %s", library_dir);
    {
        int rc = scanner_scan_cancelable(library_dir, new_catalog, &ctx->cancel_scan);

        if (rc == 1) {
            LOG_INFO("library", "background scan cancelled");
            catalog_destroy(new_catalog);
            pthread_mutex_lock(&ctx->mu);
            ctx->scanning = false;
            ctx->worker_alive = false;
            set_error(ctx, "cancelled");
            pthread_mutex_unlock(&ctx->mu);
            return NULL;
        }

        if (rc != 0) {
            LOG_ERROR("library", "background scan failed");
            catalog_destroy(new_catalog);
            pthread_mutex_lock(&ctx->mu);
            ctx->scanning = false;
            ctx->worker_alive = false;
            ctx->last_scan_ok = false;
            set_error(ctx, "scan failed");
            pthread_mutex_unlock(&ctx->mu);
            return NULL;
        }
    }

    if (catalog_adopt_ids(new_catalog, old_catalog) != 0) {
        catalog_destroy(new_catalog);
        pthread_mutex_lock(&ctx->mu);
        ctx->scanning = false;
        ctx->worker_alive = false;
        ctx->last_scan_ok = false;
        set_error(ctx, "id adopt failed");
        pthread_mutex_unlock(&ctx->mu);
        return NULL;
    }

    new_browse = browse_index_build(new_catalog);
    if (new_browse == NULL) {
        catalog_destroy(new_catalog);
        pthread_mutex_lock(&ctx->mu);
        ctx->scanning = false;
        ctx->worker_alive = false;
        ctx->last_scan_ok = false;
        set_error(ctx, "browse index failed");
        pthread_mutex_unlock(&ctx->mu);
        return NULL;
    }

    if (catalog_db_path != NULL && catalog_db_path[0] != '\0') {
        if (catalog_store_save(catalog_db_path, library_dir, new_catalog) != 0) {
            LOG_WARN("library", "failed to save catalog snapshot: %s", catalog_db_path);
        } else {
            LOG_INFO("library", "saved catalog snapshot: %s", catalog_db_path);
        }
    }

    pthread_mutex_lock(&ctx->mu);
    old_catalog = ctx->catalog;
    old_browse = ctx->browse;
    ctx->catalog = new_catalog;
    ctx->browse = new_browse;
    ctx->scanning = false;
    ctx->worker_alive = false;
    ctx->last_scan_unix = time(NULL);
    ctx->last_scan_ok = true;
    set_error(ctx, "");
    LOG_INFO("library", "background scan complete: %zu items (%zu artists, %zu albums)",
             catalog_count(new_catalog), browse_artist_count(new_browse),
             browse_album_count(new_browse));
    pthread_mutex_unlock(&ctx->mu);

    catalog_destroy(old_catalog);
    browse_index_destroy(old_browse);
    return NULL;
}

int library_scan_request(app_context_t *ctx, bool force)
{
    bool restarted = false;

    if (ctx == NULL || ctx->library_dir == NULL || ctx->library_dir[0] == '\0') {
        return -1;
    }

    pthread_mutex_lock(&ctx->mu);

    if (ctx->scanning) {
        if (!force) {
            pthread_mutex_unlock(&ctx->mu);
            return 1;
        }

        LOG_INFO("library", "force scan: cancelling in-progress scan");
        ctx->cancel_scan = 1;
        pthread_mutex_unlock(&ctx->mu);
        pthread_join(ctx->worker, NULL);
        pthread_mutex_lock(&ctx->mu);
        ctx->worker_alive = false;
        ctx->scanning = false;
        restarted = true;
    }

    ctx->cancel_scan = 0;
    ctx->scanning = true;
    set_error(ctx, "");

    if (pthread_create(&ctx->worker, NULL, scan_worker, ctx) != 0) {
        ctx->scanning = false;
        ctx->last_scan_ok = false;
        set_error(ctx, "failed to start scan thread");
        pthread_mutex_unlock(&ctx->mu);
        return -1;
    }

    ctx->worker_alive = true;
    pthread_mutex_unlock(&ctx->mu);
    return restarted ? 2 : 0;
}

void library_status_get(app_context_t *ctx, library_status_t *out)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));

    if (ctx == NULL) {
        return;
    }

    pthread_mutex_lock(&ctx->mu);
    out->scanning = ctx->scanning;
    out->has_library = ctx->library_dir != NULL && ctx->library_dir[0] != '\0';
    out->last_scan_unix = ctx->last_scan_unix;
    out->last_scan_ok = ctx->last_scan_ok;
    memcpy(out->last_error, ctx->last_error, sizeof(out->last_error));
    out->track_count = catalog_count_kind(ctx->catalog, MEDIA_KIND_AUDIO);
    out->image_count = catalog_count_kind(ctx->catalog, MEDIA_KIND_IMAGE);
    out->artist_count = browse_artist_count(ctx->browse);
    out->album_count = browse_album_count(ctx->browse);
    pthread_mutex_unlock(&ctx->mu);
}
