#include "media_server/api/library.h"

#include "media_server/api/context.h"
#include "media_server/http/server.h"
#include "media_server/library/runtime.h"
#include "media_server/util/string_buf.h"

#include <string.h>

void handle_library_status(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    library_status_t st;
    string_buf_t sb;
    const char *dir;

    (void)req;

    library_status_get(ctx, &st);
    dir = (ctx != NULL && ctx->library_dir != NULL) ? ctx->library_dir : "";

    if (string_buf_init(&sb, 256) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    if (string_buf_append(&sb, "{\"scanning\":") != 0 ||
        string_buf_append(&sb, st.scanning ? "true" : "false") != 0 ||
        string_buf_append(&sb, ",\"has_library\":") != 0 ||
        string_buf_append(&sb, st.has_library ? "true" : "false") != 0 ||
        string_buf_append(&sb, ",\"library_dir\":") != 0 ||
        string_buf_append_json_string(&sb, dir) != 0 ||
        string_buf_append_fmt(&sb, ",\"last_scan_unix\":%lld",
                              (long long)st.last_scan_unix) != 0 ||
        string_buf_append(&sb, ",\"last_scan_ok\":") != 0 ||
        string_buf_append(&sb, st.last_scan_ok ? "true" : "false") != 0 ||
        string_buf_append(&sb, ",\"last_error\":") != 0 ||
        string_buf_append_json_string(&sb, st.last_error) != 0 ||
        string_buf_append_fmt(
            &sb,
            ",\"track_count\":%zu,\"image_count\":%zu,\"artist_count\":%zu,"
            "\"album_count\":%zu}",
            st.track_count, st.image_count, st.artist_count, st.album_count) != 0) {
        string_buf_free(&sb);
        http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
        return;
    }

    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
}

void handle_library_scan(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    char force_buf[8];
    bool force = false;
    int rc;

    if (http_query_get(req, "force", force_buf, sizeof(force_buf)) == HTTP_QUERY_OK) {
        force = (strcmp(force_buf, "1") == 0 || strcmp(force_buf, "true") == 0 ||
                 strcmp(force_buf, "yes") == 0);
    }

    rc = library_scan_request(ctx, force);
    if (rc == -1) {
        http_reply_json(res, 400, "{\"error\":\"no_library\"}");
        return;
    }
    if (rc == 1) {
        http_reply_json(res, 409, "{\"error\":\"scan_in_progress\"}");
        return;
    }

    if (rc == 2) {
        http_reply_json(res, 202, "{\"status\":\"restarted\"}");
    } else {
        http_reply_json(res, 202, "{\"status\":\"started\"}");
    }
}
