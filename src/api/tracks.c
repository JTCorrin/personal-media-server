#include "media_server/api/tracks.h"

#include "media_server/api/context.h"
#include "media_server/http/server.h"
#include "media_server/library/catalog.h"
#include "media_server/media/kind.h"
#include "media_server/util/string_buf.h"

#include <stddef.h>

/* Append one catalog item as a JSON object. Returns 0 on success. */
static int append_track_json(string_buf_t *sb, const catalog_item_t *item)
{
    if (string_buf_append_fmt(sb, "{\"id\":%u,\"kind\":\"%s\",\"path\":", item->id,
                              media_kind_name(item->kind)) != 0) {
        return -1;
    }
    if (string_buf_append_json_string(sb, item->path) != 0) {
        return -1;
    }
    if (string_buf_append(sb, ",\"filename\":") != 0) {
        return -1;
    }
    if (string_buf_append_json_string(sb, item->filename) != 0) {
        return -1;
    }
    return string_buf_append_char(sb, '}');
}

void handle_tracks(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = match->user_data;
    catalog_t *catalog = ctx != NULL ? ctx->catalog : NULL;
    size_t count = catalog_count(catalog);
    string_buf_t sb;

    (void)req;

    if (string_buf_init(&sb, 64 + count * 128) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    if (string_buf_append_char(&sb, '[') != 0) {
        goto fail;
    }

    for (size_t i = 0; i < count; i++) {
        const catalog_item_t *item = catalog_get(catalog, i);

        if (item == NULL) {
            continue;
        }

        if (i > 0 && string_buf_append_char(&sb, ',') != 0) {
            goto fail;
        }
        if (append_track_json(&sb, item) != 0) {
            goto fail;
        }
    }

    if (string_buf_append_char(&sb, ']') != 0) {
        goto fail;
    }

    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
    return;

fail:
    string_buf_free(&sb);
    http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
}
