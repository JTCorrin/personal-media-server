#include "media_server/api/artists.h"

#include "media_server/api/browse_json.h"
#include "media_server/api/context.h"
#include "media_server/api/page_json.h"
#include "media_server/api/params.h"
#include "media_server/http/server.h"
#include "media_server/library/browse.h"
#include "media_server/util/string_buf.h"

#include <stdint.h>

void handle_artists(const router_match_t *match, void *req, void *res)
{
    const browse_index_t *index = api_context_browse(match);
    api_page_t page;
    string_buf_t sb;
    size_t total = 0;
    size_t written = 0;
    size_t count;

    api_page_from_req(req, &page);
    count = browse_artist_count(index);

    if (string_buf_init(&sb, 64 + page.limit * 96) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    if (page_json_begin(&sb) != 0) {
        goto fail;
    }

    for (size_t i = 0; i < count; i++) {
        const browse_artist_t *artist = browse_artist_get(index, i);
        if (artist == NULL) {
            continue;
        }
        if (total >= page.offset && written < page.limit) {
            if (written > 0 && string_buf_append_char(&sb, ',') != 0) {
                goto fail;
            }
            if (append_artist_json(&sb, artist) != 0) {
                goto fail;
            }
            written++;
        }
        total++;
    }

    if (page_json_end(&sb, total, &page) != 0) {
        goto fail;
    }

    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
    return;

fail:
    string_buf_free(&sb);
    http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
}

void handle_artist_by_id(const router_match_t *match, void *req, void *res)
{
    const browse_index_t *index = api_context_browse(match);
    uint32_t id = 0;
    const browse_artist_t *artist;
    string_buf_t sb;

    (void)req;

    if (api_parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }

    artist = browse_artist_find_id(index, id);
    if (artist == NULL) {
        http_reply_not_found(res);
        return;
    }

    if (string_buf_init(&sb, 128) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    if (append_artist_json(&sb, artist) != 0) {
        string_buf_free(&sb);
        http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
        return;
    }

    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
}

void handle_artist_albums(const router_match_t *match, void *req, void *res)
{
    const browse_index_t *index = api_context_browse(match);
    uint32_t id = 0;
    const browse_artist_t *artist;
    api_page_t page;
    string_buf_t sb;
    size_t total = 0;
    size_t written = 0;

    if (api_parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }

    artist = browse_artist_find_id(index, id);
    if (artist == NULL) {
        http_reply_not_found(res);
        return;
    }

    api_page_from_req(req, &page);

    if (string_buf_init(&sb, 64 + page.limit * 128) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    if (page_json_begin(&sb) != 0) {
        goto fail;
    }

    for (size_t i = 0; i < browse_album_count(index); i++) {
        const browse_album_t *album = browse_album_get(index, i);
        if (album == NULL || album->artist_id != id) {
            continue;
        }
        if (total >= page.offset && written < page.limit) {
            if (written > 0 && string_buf_append_char(&sb, ',') != 0) {
                goto fail;
            }
            if (append_album_json(&sb, album) != 0) {
                goto fail;
            }
            written++;
        }
        total++;
    }

    if (page_json_end(&sb, total, &page) != 0) {
        goto fail;
    }

    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
    return;

fail:
    string_buf_free(&sb);
    http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
}
