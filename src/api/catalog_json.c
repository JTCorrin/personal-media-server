#include "media_server/api/catalog_json.h"

#include "media_server/api/page_json.h"
#include "media_server/api/params.h"
#include "media_server/http/server.h"
#include "media_server/media/kind.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

static int append_optional_string(string_buf_t *sb, const char *value)
{
    return value[0] == '\0' ? string_buf_append(sb, "null")
                            : string_buf_append_json_string(sb, value);
}

static int append_overridden_fields(string_buf_t *sb, uint32_t mask)
{
    static const struct {
        uint32_t bit;
        const char *name;
    } fields[] = {
        {METADATA_FIELD_TITLE, "title"},
        {METADATA_FIELD_ARTIST, "artist"},
        {METADATA_FIELD_ALBUM, "album"},
        {METADATA_FIELD_RELEASE_DATE, "release_date"},
        {METADATA_FIELD_GENRE, "genre"},
        {METADATA_FIELD_TRACK_NUMBER, "track_number"},
        {METADATA_FIELD_DISC_NUMBER, "disc_number"},
    };
    bool comma = false;

    if (string_buf_append_char(sb, '[') != 0) {
        return -1;
    }
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        if ((mask & fields[i].bit) == 0) {
            continue;
        }
        if (comma && string_buf_append_char(sb, ',') != 0) {
            return -1;
        }
        if (string_buf_append_json_string(sb, fields[i].name) != 0) {
            return -1;
        }
        comma = true;
    }
    return string_buf_append_char(sb, ']');
}

static int append_optional_id(string_buf_t *sb, uint32_t id)
{
    return id == 0 ? string_buf_append(sb, "null")
                   : string_buf_append_fmt(sb, "%u", id);
}

static int append_track_links(string_buf_t *sb, const catalog_item_t *item,
                              const browse_index_t *browse)
{
    browse_track_link_t link = {0};

    if (item->kind != MEDIA_KIND_AUDIO) {
        return 0;
    }
    (void)browse_track_link_find(browse, item->id, &link);
    if (string_buf_append(sb, ",\"album_id\":") != 0 ||
        append_optional_id(sb, link.album_id) != 0 ||
        string_buf_append(sb, ",\"cover_id\":") != 0 ||
        append_optional_id(sb, link.cover_id) != 0) {
        return -1;
    }
    return 0;
}

int append_catalog_item_json(string_buf_t *sb, const catalog_item_t *item,
                             const browse_index_t *browse)
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
    if (string_buf_append(sb, ",\"release_date\":") != 0 ||
        append_optional_string(sb, item->release_date) != 0 ||
        string_buf_append(sb, ",\"genre\":") != 0 ||
        append_optional_string(sb, item->genre) != 0 ||
        string_buf_append(sb, ",\"track_number\":") != 0) {
        return -1;
    }
    if (item->track_number == 0) {
        if (string_buf_append(sb, "null") != 0) {
            return -1;
        }
    } else if (string_buf_append_fmt(sb, "%u", item->track_number) != 0) {
        return -1;
    }
    if (string_buf_append(sb, ",\"disc_number\":") != 0) {
        return -1;
    }
    if (item->disc_number == 0) {
        if (string_buf_append(sb, "null") != 0) {
            return -1;
        }
    } else if (string_buf_append_fmt(sb, "%u", item->disc_number) != 0) {
        return -1;
    }
    if (append_track_links(sb, item, browse) != 0 ||
        string_buf_append(sb, ",\"overridden_fields\":") != 0 ||
        append_overridden_fields(sb, item->override_mask) != 0) {
        return -1;
    }
    return string_buf_append_char(sb, '}');
}

void api_reply_catalog_kind_list(void *req, void *res, catalog_t *catalog,
                                 const browse_index_t *browse, media_kind_t kind)
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
            if (append_catalog_item_json(&sb, item, browse) != 0) {
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
                                  catalog_t *catalog,
                                  const browse_index_t *browse,
                                  media_kind_t kind)
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

    if (append_catalog_item_json(&sb, item, browse) != 0) {
        string_buf_free(&sb);
        http_reply_json(res, 500, "{\"error\":\"encode failed\"}");
        return;
    }

    http_reply_json(res, 200, string_buf_cstr(&sb));
    string_buf_free(&sb);
}
