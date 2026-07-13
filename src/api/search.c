#include "media_server/api/search.h"

#include "media_server/api/browse_json.h"
#include "media_server/api/catalog_json.h"
#include "media_server/api/context.h"
#include "media_server/api/page_json.h"
#include "media_server/api/params.h"
#include "media_server/http/server.h"
#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"
#include "media_server/library/search.h"
#include "media_server/util/string_buf.h"

#include <stddef.h>

#define SEARCH_Q_MAX 256

static int append_paged_tracks(string_buf_t *sb, catalog_t *catalog, const char *q,
                               const api_page_t *page)
{
    size_t total = 0;
    size_t written = 0;
    size_t count = catalog_count(catalog);

    if (page_json_begin(sb) != 0) {
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        const catalog_item_t *item = catalog_get(catalog, i);
        if (item == NULL || !search_track_matches(item, q)) {
            continue;
        }
        if (total >= page->offset && written < page->limit) {
            if (written > 0 && string_buf_append_char(sb, ',') != 0) {
                return -1;
            }
            if (append_catalog_item_json(sb, item) != 0) {
                return -1;
            }
            written++;
        }
        total++;
    }

    return page_json_end(sb, total, page);
}

static int append_paged_artists(string_buf_t *sb, const browse_index_t *index,
                                const char *q, const api_page_t *page)
{
    size_t total = 0;
    size_t written = 0;

    if (page_json_begin(sb) != 0) {
        return -1;
    }

    for (size_t i = 0; i < browse_artist_count(index); i++) {
        const browse_artist_t *artist = browse_artist_get(index, i);
        if (artist == NULL || !search_artist_matches(artist, q)) {
            continue;
        }
        /* Artists/albums in search: offset always 0; cap to limit. */
        if (written < page->limit) {
            if (written > 0 && string_buf_append_char(sb, ',') != 0) {
                return -1;
            }
            if (append_artist_json(sb, artist) != 0) {
                return -1;
            }
            written++;
        }
        total++;
    }

    {
        api_page_t artist_page = {.limit = page->limit, .offset = 0};
        return page_json_end(sb, total, &artist_page);
    }
}

static int append_paged_albums(string_buf_t *sb, const browse_index_t *index,
                               const char *q, const api_page_t *page)
{
    size_t total = 0;
    size_t written = 0;

    if (page_json_begin(sb) != 0) {
        return -1;
    }

    for (size_t i = 0; i < browse_album_count(index); i++) {
        const browse_album_t *album = browse_album_get(index, i);
        if (album == NULL || !search_album_matches(album, q)) {
            continue;
        }
        if (written < page->limit) {
            if (written > 0 && string_buf_append_char(sb, ',') != 0) {
                return -1;
            }
            if (append_album_json(sb, album) != 0) {
                return -1;
            }
            written++;
        }
        total++;
    }

    {
        api_page_t album_page = {.limit = page->limit, .offset = 0};
        return page_json_end(sb, total, &album_page);
    }
}

void handle_search(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    catalog_t *catalog;
    const browse_index_t *index;
    api_page_t page;
    char q[SEARCH_Q_MAX];
    string_buf_t sb;

    switch (http_query_get(req, "q", q, sizeof(q))) {
    case HTTP_QUERY_OK:
        break;
    case HTTP_QUERY_MISSING:
        http_reply_json(res, 400, "{\"error\":\"missing_query\"}");
        return;
    default:
        /* Undecodable or longer than SEARCH_Q_MAX. */
        http_reply_json(res, 400, "{\"error\":\"invalid_query\"}");
        return;
    }

    if (q[0] == '\0') {
        http_reply_json(res, 400, "{\"error\":\"missing_query\"}");
        return;
    }

    api_page_from_req(req, &page);

    if (string_buf_init(&sb, 512) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    api_context_lock(ctx);
    catalog = api_context_catalog_locked(ctx);
    index = api_context_browse_locked(ctx);

    if (string_buf_append(&sb, "{\"q\":") != 0 ||
        string_buf_append_json_string(&sb, q) != 0 ||
        string_buf_append(&sb, ",\"tracks\":") != 0 ||
        append_paged_tracks(&sb, catalog, q, &page) != 0 ||
        string_buf_append(&sb, ",\"artists\":") != 0 ||
        append_paged_artists(&sb, index, q, &page) != 0 ||
        string_buf_append(&sb, ",\"albums\":") != 0 ||
        append_paged_albums(&sb, index, q, &page) != 0 ||
        string_buf_append_char(&sb, '}') != 0) {
        api_context_unlock(ctx);
        string_buf_free(&sb);
        http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
        return;
    }

    api_context_unlock(ctx);
    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
}
