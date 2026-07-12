#include "media_server/api/page_json.h"

int page_json_begin(string_buf_t *sb)
{
    if (sb == NULL) {
        return -1;
    }
    return string_buf_append(sb, "{\"items\":[");
}

int page_json_end(string_buf_t *sb, size_t total, const api_page_t *page)
{
    if (sb == NULL || page == NULL) {
        return -1;
    }
    return string_buf_append_fmt(sb, "],\"total\":%zu,\"limit\":%zu,\"offset\":%zu}",
                                 total, page->limit, page->offset);
}
