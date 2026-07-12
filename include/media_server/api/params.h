#ifndef MEDIA_SERVER_API_PARAMS_H
#define MEDIA_SERVER_API_PARAMS_H

#include "media_server/http/router.h"

#include <stddef.h>
#include <stdint.h>

#define API_PAGE_LIMIT_DEFAULT 50
#define API_PAGE_LIMIT_MAX 200

typedef struct api_page {
    size_t limit;
    size_t offset;
} api_page_t;

/* Parse the ":id" path param as a non-zero uint32. Returns 0 on success. */
int api_parse_id_param(const router_match_t *match, uint32_t *out_id);

/*
 * Fill out from optional limit/offset query strings (NULL = use defaults).
 * limit clamped to 1..API_PAGE_LIMIT_MAX; offset clamped to >= 0.
 */
void api_page_parse(const char *limit_str, const char *offset_str, api_page_t *out);

/* Read limit/offset from the HTTP request query string. */
void api_page_from_req(void *req, api_page_t *out);

#endif /* MEDIA_SERVER_API_PARAMS_H */
