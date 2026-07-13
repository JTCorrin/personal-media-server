#ifndef MEDIA_SERVER_LIBRARY_RUNTIME_H
#define MEDIA_SERVER_LIBRARY_RUNTIME_H

#include "media_server/api/context.h"

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

typedef struct library_status {
    bool scanning;
    bool has_library;
    time_t last_scan_unix; /* 0 if never completed a scan */
    bool last_scan_ok;
    char last_error[128]; /* "" if none */
    size_t track_count;
    size_t image_count;
    size_t artist_count;
    size_t album_count;
} library_status_t;

/* Init mutex and scan fields. catalog/browse/library_dir set by caller. */
int library_runtime_init(app_context_t *ctx);

/* Join any worker and destroy mutex. Does not free catalog/browse. */
void library_runtime_shutdown(app_context_t *ctx);

/*
 * Start a background rescan into a fresh catalog, then swap + rebuild browse.
 * force: if a scan is running, cancel it and restart; otherwise return busy.
 *
 * Returns:
 *   0  started
 *   2  restarted after cancelling an in-progress scan (force only)
 *   1  ignored — scan already in progress (only when !force)
 *  -1  error (e.g. no library_dir)
 */
int library_scan_request(app_context_t *ctx, bool force);

/*
 * Copy-on-write effective metadata updates.
 * Returns 0 on success, 1 when scan/edit busy, 2 when resource not found,
 * and -1 on allocation/persistence failure.
 */
int library_metadata_patch_track(app_context_t *ctx, uint32_t track_id,
                                 const metadata_patch_t *patch,
                                 catalog_item_t *updated);
int library_metadata_patch_album(app_context_t *ctx, uint32_t album_id,
                                 const metadata_patch_t *patch,
                                 size_t *updated_track_count);

void library_status_get(app_context_t *ctx, library_status_t *out);

#endif /* MEDIA_SERVER_LIBRARY_RUNTIME_H */
