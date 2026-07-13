#include "media_server/api/discover.h"

#include "media_server/api/catalog_json.h"
#include "media_server/api/context.h"
#include "media_server/api/page_json.h"
#include "media_server/api/params.h"
#include "media_server/http/server.h"
#include "media_server/library/catalog.h"
#include "media_server/library/user_store.h"
#include "media_server/media/kind.h"
#include "media_server/util/string_buf.h"

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static int reply_audio_ids(void *res, app_context_t *ctx, const uint32_t *ids,
                           size_t count, size_t total, const api_page_t *page)
{
    string_buf_t sb;
    size_t written = 0;

    if (string_buf_init(&sb, 256) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return -1;
    }
    if (page_json_begin(&sb) != 0) {
        goto fail;
    }

    api_context_lock(ctx);
    for (size_t i = 0; i < count; i++) {
        const catalog_item_t *item =
            catalog_find_id(api_context_catalog_locked(ctx), ids[i]);
        if (item == NULL || item->kind != MEDIA_KIND_AUDIO) {
            continue;
        }
        if (written > 0 && string_buf_append_char(&sb, ',') != 0) {
            api_context_unlock(ctx);
            goto fail;
        }
        if (append_catalog_item_json(&sb, item) != 0) {
            api_context_unlock(ctx);
            goto fail;
        }
        written++;
    }
    api_context_unlock(ctx);

    if (page_json_end(&sb, total, page) != 0) {
        goto fail;
    }
    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
    return 0;

fail:
    string_buf_free(&sb);
    http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
    return -1;
}

void handle_discover_random(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    api_page_t page;
    size_t audio_n = 0;
    size_t *indices = NULL;
    uint32_t *ids = NULL;
    size_t take;
    catalog_t *catalog;

    api_page_from_req(req, &page);
    take = page.limit;

    api_context_lock(ctx);
    catalog = api_context_catalog_locked(ctx);
    audio_n = catalog_count_kind(catalog, MEDIA_KIND_AUDIO);
    if (audio_n == 0 || take == 0) {
        api_context_unlock(ctx);
        reply_audio_ids(res, ctx, NULL, 0, 0, &page);
        return;
    }
    if (take > audio_n) {
        take = audio_n;
    }

    indices = malloc(audio_n * sizeof(*indices));
    ids = malloc(take * sizeof(*ids));
    if (indices == NULL || ids == NULL) {
        free(indices);
        free(ids);
        api_context_unlock(ctx);
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    {
        size_t ai = 0;
        for (size_t i = 0; i < catalog_count(catalog); i++) {
            const catalog_item_t *item = catalog_get(catalog, i);
            if (item != NULL && item->kind == MEDIA_KIND_AUDIO) {
                indices[ai++] = i;
            }
        }
    }

    /* Partial Fisher–Yates for first `take` slots. */
    srand((unsigned)time(NULL) ^ (unsigned)(uintptr_t)ctx);
    for (size_t i = 0; i < take; i++) {
        size_t j = i + (size_t)(rand() % (int)(audio_n - i));
        size_t tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
        ids[i] = catalog_get(catalog, indices[i])->id;
    }
    api_context_unlock(ctx);

    reply_audio_ids(res, ctx, ids, take, take, &page);
    free(indices);
    free(ids);
}

void handle_discover_recent(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    api_page_t page;
    uint32_t *ids = NULL;
    size_t audio_n;
    size_t take;
    size_t written = 0;
    catalog_t *catalog;

    api_page_from_req(req, &page);
    take = page.limit;

    api_context_lock(ctx);
    catalog = api_context_catalog_locked(ctx);
    audio_n = catalog_count_kind(catalog, MEDIA_KIND_AUDIO);
    if (audio_n == 0 || take == 0) {
        api_context_unlock(ctx);
        reply_audio_ids(res, ctx, NULL, 0, 0, &page);
        return;
    }
    if (take > audio_n) {
        take = audio_n;
    }

    ids = malloc(take * sizeof(*ids));
    if (ids == NULL) {
        api_context_unlock(ctx);
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    /* Highest ids first among audio. */
    for (size_t pass = 0; pass < take; pass++) {
        uint32_t best_id = 0;
        size_t best_i = 0;
        bool found = false;
        for (size_t i = 0; i < catalog_count(catalog); i++) {
            const catalog_item_t *item = catalog_get(catalog, i);
            bool used = false;
            if (item == NULL || item->kind != MEDIA_KIND_AUDIO) {
                continue;
            }
            for (size_t u = 0; u < written; u++) {
                if (ids[u] == item->id) {
                    used = true;
                    break;
                }
            }
            if (used) {
                continue;
            }
            if (!found || item->id > best_id) {
                best_id = item->id;
                best_i = i;
                found = true;
            }
        }
        (void)best_i;
        if (!found) {
            break;
        }
        ids[written++] = best_id;
    }
    api_context_unlock(ctx);

    reply_audio_ids(res, ctx, ids, written, written, &page);
    free(ids);
}

void handle_discover_recently_played(const router_match_t *match, void *req, void *res)
{
    app_context_t *ctx = api_context_from_match(match);
    api_page_t page;
    user_history_entry_t entries[256];
    uint32_t ids[256];
    size_t n;
    size_t take;
    size_t written = 0;

    api_page_from_req(req, &page);
    take = page.limit < 256 ? page.limit : 256;

    if (ctx == NULL || ctx->user_store == NULL) {
        reply_audio_ids(res, ctx, NULL, 0, 0, &page);
        return;
    }

    n = user_store_history_list(ctx->user_store, page.offset, take, entries, 256);
    for (size_t i = 0; i < n && written < 256; i++) {
        ids[written++] = entries[i].track_id;
    }
    reply_audio_ids(res, ctx, ids, written, user_store_history_count(ctx->user_store),
                    &page);
}
