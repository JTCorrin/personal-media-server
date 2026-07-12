#include "media_server/api/params.h"

#include "media_server/http/server.h"

#include <errno.h>
#include <stdlib.h>

int api_parse_id_param(const router_match_t *match, uint32_t *out_id)
{
    const char *raw;
    char *end = NULL;
    unsigned long long value;

    if (match == NULL || out_id == NULL) {
        return -1;
    }

    raw = router_param_get(match, "id");
    if (raw == NULL || raw[0] == '\0') {
        return -1;
    }

    /*
     * strtoull, not strtoul: unsigned long may be 32-bit, which would make
     * the range check below a no-op on such platforms.
     */
    errno = 0;
    value = strtoull(raw, &end, 10);
    if (errno != 0 || end == raw || *end != '\0' || value == 0 || value > UINT32_MAX) {
        return -1;
    }

    *out_id = (uint32_t)value;
    return 0;
}

static size_t parse_size_or(const char *raw, size_t fallback)
{
    char *end = NULL;
    unsigned long long value;

    if (raw == NULL || raw[0] == '\0') {
        return fallback;
    }

    errno = 0;
    value = strtoull(raw, &end, 10);
    if (errno != 0 || end == raw || *end != '\0') {
        return fallback;
    }

    return (size_t)value;
}

void api_page_parse(const char *limit_str, const char *offset_str, api_page_t *out)
{
    size_t limit;
    size_t offset;

    if (out == NULL) {
        return;
    }

    limit = parse_size_or(limit_str, API_PAGE_LIMIT_DEFAULT);
    offset = parse_size_or(offset_str, 0);

    if (limit < 1) {
        limit = 1;
    }
    if (limit > API_PAGE_LIMIT_MAX) {
        limit = API_PAGE_LIMIT_MAX;
    }

    out->limit = limit;
    out->offset = offset;
}

void api_page_from_req(void *req, api_page_t *out)
{
    char limit_buf[32];
    char offset_buf[32];
    const char *limit_str = NULL;
    const char *offset_str = NULL;

    if (http_query_get(req, "limit", limit_buf, sizeof(limit_buf)) == 0) {
        limit_str = limit_buf;
    }
    if (http_query_get(req, "offset", offset_buf, sizeof(offset_buf)) == 0) {
        offset_str = offset_buf;
    }

    api_page_parse(limit_str, offset_str, out);
}
