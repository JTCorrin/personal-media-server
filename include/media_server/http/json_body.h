#ifndef MEDIA_SERVER_HTTP_JSON_BODY_H
#define MEDIA_SERVER_HTTP_JSON_BODY_H

#include <stddef.h>
#include <stdint.h>

/*
 * Minimal JSON body helpers for the shapes we accept:
 *   {"name":"..."}
 *   {"track_id":123}
 *   {"track_ids":[1,2,3]}
 *
 * Whitespace-tolerant. No general-purpose JSON parser.
 */

/* Pointer + length view of the HTTP body (may not be NUL-terminated). */
int http_body_view(void *req, const char **out_buf, size_t *out_len);

/* Parse string field "name" from a JSON object body. Returns 0 on success. */
int http_json_get_string_field(const char *body, size_t body_len, const char *key,
                               char *out, size_t out_size);

/* Parse unsigned integer field. Returns 0 on success. */
int http_json_get_u32_field(const char *body, size_t body_len, const char *key,
                            uint32_t *out);

#define HTTP_JSON_FIELD_INVALID -1
#define HTTP_JSON_FIELD_ABSENT 0
#define HTTP_JSON_FIELD_VALUE 1
#define HTTP_JSON_FIELD_NULL 2

/* Tri-state helpers for PATCH: absent, concrete value, explicit null. */
int http_json_get_optional_string(const char *body, size_t body_len,
                                  const char *key, char *out,
                                  size_t out_size);
int http_json_get_optional_u32(const char *body, size_t body_len,
                               const char *key, uint32_t *out);

/*
 * Parse array of u32 under key. Writes up to max_out values.
 * *out_count is set to number written. Returns 0 on success.
 */
int http_json_get_u32_array(const char *body, size_t body_len, const char *key,
                            uint32_t *out, size_t max_out, size_t *out_count);

/* Convenience wrappers that read the HTTP request body first. */
int http_req_json_get_string(void *req, const char *key, char *out, size_t out_size);
int http_req_json_get_u32(void *req, const char *key, uint32_t *out);
int http_req_json_get_optional_string(void *req, const char *key, char *out,
                                      size_t out_size);
int http_req_json_get_optional_u32(void *req, const char *key, uint32_t *out);
int http_req_json_get_u32_array(void *req, const char *key, uint32_t *out,
                                size_t max_out, size_t *out_count);

#endif /* MEDIA_SERVER_HTTP_JSON_BODY_H */
