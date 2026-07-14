#include "media_server/library/runtime.h"

#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"
#include "media_server/library/catalog_store.h"
#include "media_server/library/scanner.h"
#include "media_server/media/file.h"
#include "media_server/media/kind.h"
#include "media_server/util/log.h"
#include "media_server/util/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int library_runtime_init(app_context_t *ctx)
{
    if (ctx == NULL) {
        return -1;
    }

    if (pthread_mutex_init(&ctx->mu, NULL) != 0) {
        return -1;
    }

    ctx->scanning = false;
    ctx->metadata_mutating = false;
    ctx->worker_joinable = false;
    atomic_init(&ctx->cancel_scan, false);
    ctx->last_scan_unix = 0;
    ctx->last_scan_ok = false;
    ctx->last_error[0] = '\0';
    return 0;
}

void library_runtime_shutdown(app_context_t *ctx)
{
    bool join_worker = false;
    pthread_t worker = {0};

    if (ctx == NULL) {
        return;
    }

    pthread_mutex_lock(&ctx->mu);
    if (ctx->worker_joinable) {
        atomic_store(&ctx->cancel_scan, true);
        worker = ctx->worker;
        join_worker = true;
    }
    pthread_mutex_unlock(&ctx->mu);

    if (join_worker) {
        pthread_join(worker, NULL);
    }

    pthread_mutex_lock(&ctx->mu);
    ctx->worker_joinable = false;
    ctx->scanning = false;
    ctx->metadata_mutating = false;
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
            ctx->last_scan_ok = false;
            set_error(ctx, "cancelled");
            pthread_mutex_unlock(&ctx->mu);
            return NULL;
        }

        if (rc != 0) {
            LOG_ERROR("library", "background scan failed");
            catalog_destroy(new_catalog);
            pthread_mutex_lock(&ctx->mu);
            ctx->scanning = false;
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
        ctx->last_scan_ok = false;
        set_error(ctx, "id adopt failed");
        pthread_mutex_unlock(&ctx->mu);
        return NULL;
    }

    if (catalog_db_path != NULL && catalog_db_path[0] != '\0' &&
        catalog_store_apply_overrides(catalog_db_path, new_catalog) != 0) {
        catalog_destroy(new_catalog);
        pthread_mutex_lock(&ctx->mu);
        ctx->scanning = false;
        ctx->last_scan_ok = false;
        set_error(ctx, "override load failed");
        pthread_mutex_unlock(&ctx->mu);
        return NULL;
    }

    new_browse = browse_index_build(new_catalog);
    if (new_browse == NULL) {
        catalog_destroy(new_catalog);
        pthread_mutex_lock(&ctx->mu);
        ctx->scanning = false;
        ctx->last_scan_ok = false;
        set_error(ctx, "browse index failed");
        pthread_mutex_unlock(&ctx->mu);
        return NULL;
    }

    if (catalog_db_path != NULL && catalog_db_path[0] != '\0') {
        if (catalog_store_save(catalog_db_path, library_dir, new_catalog) != 0) {
            LOG_ERROR("library", "failed to save catalog snapshot: %s",
                      catalog_db_path);
            browse_index_destroy(new_browse);
            catalog_destroy(new_catalog);
            pthread_mutex_lock(&ctx->mu);
            ctx->scanning = false;
            ctx->last_scan_ok = false;
            set_error(ctx, "snapshot save failed");
            pthread_mutex_unlock(&ctx->mu);
            return NULL;
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
    bool reap_worker = false;
    pthread_t worker = {0};

    if (ctx == NULL || ctx->library_dir == NULL || ctx->library_dir[0] == '\0') {
        return -1;
    }

    pthread_mutex_lock(&ctx->mu);

    if (ctx->metadata_mutating) {
        pthread_mutex_unlock(&ctx->mu);
        return 1;
    }

    if (ctx->scanning) {
        if (!force) {
            pthread_mutex_unlock(&ctx->mu);
            return 1;
        }

        LOG_INFO("library", "force scan: cancelling in-progress scan");
        atomic_store(&ctx->cancel_scan, true);
        worker = ctx->worker;
        reap_worker = true;
        restarted = true;
    } else if (ctx->worker_joinable) {
        /* A completed joinable thread still owns pthread resources. */
        worker = ctx->worker;
        reap_worker = true;
    }

    if (reap_worker) {
        pthread_mutex_unlock(&ctx->mu);
        pthread_join(worker, NULL);
        pthread_mutex_lock(&ctx->mu);
        ctx->worker_joinable = false;
        ctx->scanning = false;
    }

    atomic_store(&ctx->cancel_scan, false);
    ctx->scanning = true;
    set_error(ctx, "");

    if (pthread_create(&ctx->worker, NULL, scan_worker, ctx) != 0) {
        ctx->scanning = false;
        ctx->last_scan_ok = false;
        set_error(ctx, "failed to start scan thread");
        pthread_mutex_unlock(&ctx->mu);
        return -1;
    }

    ctx->worker_joinable = true;
    pthread_mutex_unlock(&ctx->mu);
    return restarted ? 2 : 0;
}

static void metadata_mutation_finish(app_context_t *ctx, catalog_t *new_catalog,
                                     browse_index_t *new_browse)
{
    catalog_t *old_catalog;
    browse_index_t *old_browse;

    pthread_mutex_lock(&ctx->mu);
    old_catalog = ctx->catalog;
    old_browse = ctx->browse;
    ctx->catalog = new_catalog;
    ctx->browse = new_browse;
    ctx->metadata_mutating = false;
    pthread_mutex_unlock(&ctx->mu);

    catalog_destroy(old_catalog);
    browse_index_destroy(old_browse);
}

static void metadata_mutation_abort(app_context_t *ctx)
{
    pthread_mutex_lock(&ctx->mu);
    ctx->metadata_mutating = false;
    pthread_mutex_unlock(&ctx->mu);
}

static int metadata_mutation_begin(app_context_t *ctx, catalog_t **copy)
{
    if (ctx == NULL || copy == NULL) {
        return -1;
    }
    pthread_mutex_lock(&ctx->mu);
    if (ctx->scanning || ctx->metadata_mutating) {
        pthread_mutex_unlock(&ctx->mu);
        return 1;
    }
    ctx->metadata_mutating = true;
    *copy = catalog_clone(ctx->catalog);
    pthread_mutex_unlock(&ctx->mu);
    if (*copy == NULL) {
        metadata_mutation_abort(ctx);
        return -1;
    }
    return 0;
}

int library_metadata_patch_track(app_context_t *ctx, uint32_t track_id,
                                 const metadata_patch_t *patch,
                                 catalog_item_t *updated)
{
    catalog_t *copy = NULL;
    browse_index_t *browse = NULL;
    const catalog_item_t *item;
    const char *paths[1];
    int rc;

    if (patch == NULL) {
        return -1;
    }
    rc = metadata_mutation_begin(ctx, &copy);
    if (rc != 0) {
        return rc;
    }
    item = catalog_find_id(copy, track_id);
    if (item == NULL || item->kind != MEDIA_KIND_AUDIO) {
        catalog_destroy(copy);
        metadata_mutation_abort(ctx);
        return 2;
    }
    paths[0] = item->path;
    if (catalog_apply_metadata_patch(copy, track_id, patch) != 0) {
        catalog_destroy(copy);
        metadata_mutation_abort(ctx);
        return -1;
    }
    browse = browse_index_build(copy);
    if (browse == NULL) {
        catalog_destroy(copy);
        metadata_mutation_abort(ctx);
        return -1;
    }
    if (ctx->catalog_db_path != NULL && ctx->catalog_db_path[0] != '\0' &&
        catalog_store_patch_overrides(ctx->catalog_db_path, paths, 1, patch) != 0) {
        browse_index_destroy(browse);
        catalog_destroy(copy);
        metadata_mutation_abort(ctx);
        return -1;
    }
    item = catalog_find_id(copy, track_id);
    if (updated != NULL) {
        *updated = *item;
    }
    metadata_mutation_finish(ctx, copy, browse);
    return 0;
}

int library_metadata_patch_album(app_context_t *ctx, uint32_t album_id,
                                 const metadata_patch_t *patch,
                                 size_t *updated_track_count)
{
    catalog_t *copy = NULL;
    browse_index_t *browse = NULL;
    browse_album_t album;
    const char **paths = NULL;
    uint32_t *ids = NULL;
    size_t count = 0;

    if (ctx == NULL || patch == NULL || updated_track_count == NULL) {
        return -1;
    }
    *updated_track_count = 0;

    pthread_mutex_lock(&ctx->mu);
    if (ctx->scanning || ctx->metadata_mutating) {
        pthread_mutex_unlock(&ctx->mu);
        return 1;
    }
    {
        const browse_album_t *found = browse_album_find_id(ctx->browse, album_id);
        if (found == NULL) {
            pthread_mutex_unlock(&ctx->mu);
            return 2;
        }
        album = *found;
    }
    ctx->metadata_mutating = true;
    copy = catalog_clone(ctx->catalog);
    pthread_mutex_unlock(&ctx->mu);
    if (copy == NULL) {
        metadata_mutation_abort(ctx);
        return -1;
    }

    for (size_t i = 0; i < catalog_count(copy); i++) {
        const catalog_item_t *item = catalog_get(copy, i);
        if (browse_album_owns_item(&album, item)) {
            count++;
        }
    }
    if (count == 0) {
        catalog_destroy(copy);
        metadata_mutation_abort(ctx);
        return 2;
    }
    paths = calloc(count, sizeof(*paths));
    ids = calloc(count, sizeof(*ids));
    if (paths == NULL || ids == NULL) {
        free(paths);
        free(ids);
        catalog_destroy(copy);
        metadata_mutation_abort(ctx);
        return -1;
    }
    count = 0;
    for (size_t i = 0; i < catalog_count(copy); i++) {
        const catalog_item_t *item = catalog_get(copy, i);
        if (!browse_album_owns_item(&album, item)) {
            continue;
        }
        paths[count] = item->path;
        ids[count] = item->id;
        count++;
    }
    for (size_t i = 0; i < count; i++) {
        if (catalog_apply_metadata_patch(copy, ids[i], patch) != 0) {
            free(paths);
            free(ids);
            catalog_destroy(copy);
            metadata_mutation_abort(ctx);
            return -1;
        }
    }
    browse = browse_index_build(copy);
    if (browse == NULL) {
        free(paths);
        free(ids);
        catalog_destroy(copy);
        metadata_mutation_abort(ctx);
        return -1;
    }
    if (ctx->catalog_db_path != NULL && ctx->catalog_db_path[0] != '\0' &&
        catalog_store_patch_overrides(ctx->catalog_db_path, paths, count, patch) != 0) {
        free(paths);
        free(ids);
        browse_index_destroy(browse);
        catalog_destroy(copy);
        metadata_mutation_abort(ctx);
        return -1;
    }

    free(paths);
    free(ids);
    *updated_track_count = count;
    metadata_mutation_finish(ctx, copy, browse);
    return 0;
}

static int resolve_album_cover_dir(app_context_t *ctx, uint32_t album_id,
                                   char *dir_out, size_t dir_out_size)
{
    const browse_album_t *album;
    catalog_t *catalog;
    char common_dir[CATALOG_PATH_MAX];
    bool have_dir = false;
    size_t count;

    album = browse_album_find_id(ctx->browse, album_id);
    if (album == NULL) {
        return 3;
    }

    catalog = ctx->catalog;
    count = catalog_count(catalog);
    common_dir[0] = '\0';

    for (size_t i = 0; i < count; i++) {
        const catalog_item_t *item = catalog_get(catalog, i);
        char dir[CATALOG_PATH_MAX];

        if (item == NULL || item->kind != MEDIA_KIND_AUDIO ||
            !browse_album_owns_item(album, item)) {
            continue;
        }
        if (path_dirname(dir, sizeof(dir), item->path) != 0) {
            return -1;
        }
        if (dir[0] == '\0') {
            return 5;
        }
        if (!have_dir) {
            if (strlen(dir) + 1 > sizeof(common_dir)) {
                return -1;
            }
            memcpy(common_dir, dir, strlen(dir) + 1);
            have_dir = true;
        } else if (strcmp(common_dir, dir) != 0) {
            char next_common[CATALOG_PATH_MAX];

            if (path_common_dir(next_common, sizeof(next_common), common_dir,
                                dir) != 0) {
                return -1;
            }
            if (next_common[0] == '\0') {
                return 6;
            }
            memcpy(common_dir, next_common, strlen(next_common) + 1);
        }
    }

    if (!have_dir) {
        return 5;
    }
    if (strlen(common_dir) + 1 > dir_out_size) {
        return -1;
    }
    memcpy(dir_out, common_dir, strlen(common_dir) + 1);
    return 0;
}

static int atomic_write_file(const char *abs_path, const void *bytes, size_t len)
{
    char tmp_path[CATALOG_PATH_MAX * 2 + 64];
    FILE *fp;
    size_t written;
    int fd;

    if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%d", abs_path, (int)getpid()) >=
        (int)sizeof(tmp_path)) {
        return -1;
    }

    fp = fopen(tmp_path, "wb");
    if (fp == NULL) {
        return -1;
    }
    written = fwrite(bytes, 1, len, fp);
    if (written != len) {
        fclose(fp);
        remove(tmp_path);
        return -1;
    }
    if (fflush(fp) != 0) {
        fclose(fp);
        remove(tmp_path);
        return -1;
    }
    fd = fileno(fp);
    if (fd >= 0 && fsync(fd) != 0) {
        fclose(fp);
        remove(tmp_path);
        return -1;
    }
    if (fclose(fp) != 0) {
        remove(tmp_path);
        return -1;
    }
    if (rename(tmp_path, abs_path) != 0) {
        remove(tmp_path);
        return -1;
    }
    return 0;
}

int library_album_cover_put(app_context_t *ctx, uint32_t album_id,
                            const void *bytes, size_t len, const char *ext,
                            char *out_rel_path, size_t out_rel_path_size)
{
    char rel_dir[CATALOG_PATH_MAX];
    char rel_path[CATALOG_PATH_MAX];
    char abs_path[CATALOG_PATH_MAX * 2];
    char abs_dir[CATALOG_PATH_MAX * 2];
    struct stat st;
    int rc;
    int scan_rc;

    if (ctx == NULL || bytes == NULL || ext == NULL || ext[0] == '\0' ||
        (ext[0] == '.' && ext[1] == '\0')) {
        return -1;
    }
    if (len == 0 || len > LIBRARY_COVER_MAX_BYTES) {
        return -1;
    }

    /* Normalize ext: allow callers to pass ".jpg" or "jpg". */
    if (ext[0] == '.') {
        ext++;
    }

    pthread_mutex_lock(&ctx->mu);
    if (ctx->library_dir == NULL || ctx->library_dir[0] == '\0') {
        pthread_mutex_unlock(&ctx->mu);
        return 4;
    }
    if (ctx->scanning || ctx->metadata_mutating) {
        pthread_mutex_unlock(&ctx->mu);
        return 1;
    }

    rc = resolve_album_cover_dir(ctx, album_id, rel_dir, sizeof(rel_dir));
    if (rc != 0) {
        pthread_mutex_unlock(&ctx->mu);
        return rc;
    }

    if (snprintf(rel_path, sizeof(rel_path), "%s/cover.%s", rel_dir, ext) >=
        (int)sizeof(rel_path)) {
        pthread_mutex_unlock(&ctx->mu);
        return -1;
    }
    if (media_resolve_path(ctx->library_dir, rel_path, abs_path, sizeof(abs_path)) !=
        0) {
        pthread_mutex_unlock(&ctx->mu);
        return -1;
    }
    if (path_dirname(abs_dir, sizeof(abs_dir), abs_path) != 0 || abs_dir[0] == '\0') {
        pthread_mutex_unlock(&ctx->mu);
        return -1;
    }
    if (stat(abs_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        pthread_mutex_unlock(&ctx->mu);
        return 5;
    }

    ctx->metadata_mutating = true;
    pthread_mutex_unlock(&ctx->mu);

    if (atomic_write_file(abs_path, bytes, len) != 0) {
        metadata_mutation_abort(ctx);
        LOG_ERROR("library", "failed to write album cover %s", abs_path);
        return -1;
    }

    metadata_mutation_abort(ctx);

    if (out_rel_path != NULL && out_rel_path_size > 0) {
        if (strlen(rel_path) + 1 > out_rel_path_size) {
            return -1;
        }
        memcpy(out_rel_path, rel_path, strlen(rel_path) + 1);
    }

    LOG_INFO("library", "wrote album cover %s; requesting rescan", rel_path);
    scan_rc = library_scan_request(ctx, false);
    if (scan_rc == 0 || scan_rc == 2) {
        return scan_rc;
    }
    if (scan_rc == 1) {
        return 1;
    }
    return -1;
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
