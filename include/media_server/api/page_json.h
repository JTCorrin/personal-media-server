#ifndef MEDIA_SERVER_API_PAGE_JSON_H
#define MEDIA_SERVER_API_PAGE_JSON_H

#include "media_server/api/params.h"
#include "media_server/util/string_buf.h"

#include <stddef.h>

/* Start {"items":[ */
int page_json_begin(string_buf_t *sb);

/* Finish ],"total":N,"limit":L,"offset":O} */
int page_json_end(string_buf_t *sb, size_t total, const api_page_t *page);

#endif /* MEDIA_SERVER_API_PAGE_JSON_H */
