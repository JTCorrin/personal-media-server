#include "media_server/api/catalog_json.h"

#include "media_server/api/page_json.h"
#include "media_server/api/params.h"
#include "media_server/http/server.h"
#include "media_server/media/kind.h"

#include <stdint.h>
#include <stddef.h>

int append_catalog_item_json(string_buf_t *sb, const catalog_item_t *item)
{
    if (sb == NULL || item == NULL) {
        return -1;
    }

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
    if (string_buf_append(sb, ",\"artist\":") != 0) {
        return -1;
    }
    if (string_buf_append_json_string(sb, item->artist) != 0) {
        return -1;
    }
    if (string_buf_append(sb, ",\"album\":") != 0) {
        return -1;
    }
    if (string_buf_append_json_string(sb, item->album) != 0) {
        return -1;
    }
    if (string_buf_append(sb, ",\"title\":") != 0) {
        return -1;
    }
    if (string_buf_append_json_string(sb, item->title) != 0) {
        return -1;
    }
    return string_buf_append_char(sb, '}');
}

void api_reply_catalog_kind_list(void *req, void *res, catalog_t *catalog,
                                 media_kind_t kind)
{
    api_page_t page;
    size_t count = catalog_count(catalog);
    size_t total = 0;
    size_t written = 0;
    string_buf_t sb;

    api_page_from_req(req, &page);

    if (string_buf_init(&sb, 64 + page.limit * 160) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    if (page_json_begin(&sb) != 0) {
        goto fail;
    }

    for (size_t i = 0; i < count; i++) {
        const catalog_item_t *item = catalog_get(catalog, i);

        if (item == NULL || item->kind != kind) {
            continue;
        }

        if (total >= page.offset && written < page.limit) {
            if (written > 0 && string_buf_append_char(&sb, ',') != 0) {
                goto fail;
            }
            if (append_catalog_item_json(&sb, item) != 0) {
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

void api_reply_catalog_kind_by_id(const router_match_t *match, void *res,
                                  catalog_t *catalog, media_kind_t kind)
{
    uint32_t id = 0;
    const catalog_item_t *item;
    string_buf_t sb;

    if (api_parse_id_param(match, &id) != 0) {
        http_reply_not_found(res);
        return;
    }

    item = catalog_find_id(catalog, id);
    if (item == NULL || item->kind != kind) {
        http_reply_not_found(res);
        return;
    }

    if (string_buf_init(&sb, 256) != 0) {
        http_reply_json(res, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    if (append_catalog_item_json(&sb, item) != 0) {
        string_buf_free(&sb);
        http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
        return;
    }

    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
}
