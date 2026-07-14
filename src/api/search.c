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

#include <stdlib.h>
#include <string.h>

#define SEARCH_Q_MAX 256

typedef struct scored_ptr {
    const void *ptr;
    int score;
    uint32_t id; /* stable tie-break */
} scored_ptr_t;

static int scored_ptr_cmp(const void *a, const void *b)
{
    const scored_ptr_t *sa = a;
    const scored_ptr_t *sb = b;

    if (sa->score != sb->score) {
        return sb->score - sa->score; /* higher score first */
    }
    if (sa->id < sb->id) {
        return -1;
    }
    if (sa->id > sb->id) {
        return 1;
    }
    return 0;
}

static int append_scored_page(string_buf_t *sb, scored_ptr_t *hits, size_t total,
                              const api_page_t *page,
                              const browse_index_t *browse,
                              int (*append_one)(string_buf_t *, const void *,
                                                const browse_index_t *))
{
    size_t written = 0;

    if (hits != NULL && total > 1) {
        qsort(hits, total, sizeof(*hits), scored_ptr_cmp);
    }

    if (page_json_begin(sb) != 0) {
        return -1;
    }

    for (size_t i = page->offset; i < total && written < page->limit; i++) {
        if (written > 0 && string_buf_append_char(sb, ',') != 0) {
            return -1;
        }
        if (append_one(sb, hits[i].ptr, browse) != 0) {
            return -1;
        }
        written++;
    }

    return page_json_end(sb, total, page);
}

static int append_track_wrap(string_buf_t *sb, const void *ptr,
                             const browse_index_t *browse)
{
    return append_catalog_item_json(sb, ptr, browse);
}

static int append_artist_wrap(string_buf_t *sb, const void *ptr,
                              const browse_index_t *browse)
{
    (void)browse;
    return append_artist_json(sb, ptr);
}

static int append_album_wrap(string_buf_t *sb, const void *ptr,
                             const browse_index_t *browse)
{
    (void)browse;
    return append_album_json(sb, ptr);
}

static int append_paged_tracks(string_buf_t *sb, catalog_t *catalog, const char *q,
                               bool fuzzy, const api_page_t *page,
                               const browse_index_t *browse)
{
    size_t count = catalog_count(catalog);
    scored_ptr_t *hits = NULL;
    size_t total = 0;
    size_t cap = 0;
    int rc;

    for (size_t i = 0; i < count; i++) {
        const catalog_item_t *item = catalog_get(catalog, i);
        int score;

        if (item == NULL) {
            continue;
        }
        score = search_track_score(item, q, fuzzy);
        if (score <= 0) {
            continue;
        }
        if (total == cap) {
            size_t ncap = cap == 0 ? 64 : cap * 2;
            scored_ptr_t *grown = realloc(hits, ncap * sizeof(*hits));
            if (grown == NULL) {
                free(hits);
                return -1;
            }
            hits = grown;
            cap = ncap;
        }
        hits[total].ptr = item;
        hits[total].score = score;
        hits[total].id = item->id;
        total++;
    }

    rc = append_scored_page(sb, hits, total, page, browse, append_track_wrap);
    free(hits);
    return rc;
}

static int append_paged_artists(string_buf_t *sb, const browse_index_t *index,
                                const char *q, bool fuzzy, const api_page_t *page)
{
    size_t count = browse_artist_count(index);
    scored_ptr_t *hits = NULL;
    size_t total = 0;
    size_t cap = 0;
    int rc;

    for (size_t i = 0; i < count; i++) {
        const browse_artist_t *artist = browse_artist_get(index, i);
        int score;

        if (artist == NULL) {
            continue;
        }
        score = search_artist_score(artist, q, fuzzy);
        if (score <= 0) {
            continue;
        }
        if (total == cap) {
            size_t ncap = cap == 0 ? 32 : cap * 2;
            scored_ptr_t *grown = realloc(hits, ncap * sizeof(*hits));
            if (grown == NULL) {
                free(hits);
                return -1;
            }
            hits = grown;
            cap = ncap;
        }
        hits[total].ptr = artist;
        hits[total].score = score;
        hits[total].id = artist->id;
        total++;
    }

    rc = append_scored_page(sb, hits, total, page, index, append_artist_wrap);
    free(hits);
    return rc;
}

static int append_paged_albums(string_buf_t *sb, const browse_index_t *index,
                               const char *q, bool fuzzy, const api_page_t *page)
{
    size_t count = browse_album_count(index);
    scored_ptr_t *hits = NULL;
    size_t total = 0;
    size_t cap = 0;
    int rc;

    for (size_t i = 0; i < count; i++) {
        const browse_album_t *album = browse_album_get(index, i);
        int score;

        if (album == NULL) {
            continue;
        }
        score = search_album_score(album, q, fuzzy);
        if (score <= 0) {
            continue;
        }
        if (total == cap) {
            size_t ncap = cap == 0 ? 32 : cap * 2;
            scored_ptr_t *grown = realloc(hits, ncap * sizeof(*hits));
            if (grown == NULL) {
                free(hits);
                return -1;
            }
            hits = grown;
            cap = ncap;
        }
        hits[total].ptr = album;
        hits[total].score = score;
        hits[total].id = album->id;
        total++;
    }

    rc = append_scored_page(sb, hits, total, page, index, append_album_wrap);
    free(hits);
    return rc;
}

void handle_search(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    catalog_t *catalog;
    const browse_index_t *index;
    api_page_t page;
    char q[SEARCH_Q_MAX];
    char fuzzy_buf[8];
    bool fuzzy = false;
    string_buf_t sb;

    switch (http_query_get(req, "q", q, sizeof(q))) {
    case HTTP_QUERY_OK:
        break;
    case HTTP_QUERY_MISSING:
        http_reply_json(res, 400, "{\"error\":\"missing_query\"}");
        return;
    default:
        http_reply_json(res, 400, "{\"error\":\"invalid_query\"}");
        return;
    }

    if (q[0] == '\0') {
        http_reply_json(res, 400, "{\"error\":\"missing_query\"}");
        return;
    }

    if (http_query_get(req, "fuzzy", fuzzy_buf, sizeof(fuzzy_buf)) == HTTP_QUERY_OK) {
        fuzzy = (strcmp(fuzzy_buf, "1") == 0 || strcmp(fuzzy_buf, "true") == 0 ||
                 strcmp(fuzzy_buf, "yes") == 0);
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
        string_buf_append(&sb, ",\"fuzzy\":") != 0 ||
        string_buf_append(&sb, fuzzy ? "true" : "false") != 0 ||
        string_buf_append(&sb, ",\"tracks\":") != 0 ||
        append_paged_tracks(&sb, catalog, q, fuzzy, &page, index) != 0 ||
        string_buf_append(&sb, ",\"artists\":") != 0 ||
        append_paged_artists(&sb, index, q, fuzzy, &page) != 0 ||
        string_buf_append(&sb, ",\"albums\":") != 0 ||
        append_paged_albums(&sb, index, q, fuzzy, &page) != 0 ||
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
