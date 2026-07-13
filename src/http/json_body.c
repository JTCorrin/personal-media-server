#include "media_server/http/json_body.h"

#include "mongoose.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int json_object_view(const char *body, size_t body_len, struct mg_str *out)
{
    int token_len = 0;
    int offset;

    if (body == NULL || out == NULL) {
        return -1;
    }

    while (body_len > 0 && isspace((unsigned char)*body)) {
        body++;
        body_len--;
    }
    while (body_len > 0 && isspace((unsigned char)body[body_len - 1])) {
        body_len--;
    }
    if (body_len < 2 || body[0] != '{' || body[body_len - 1] != '}') {
        return -1;
    }

    *out = mg_str_n(body, body_len);
    offset = mg_json_get(*out, "$", &token_len);
    if (offset != 0 || token_len < 0 || (size_t)token_len != body_len) {
        return -1;
    }
    return 0;
}

int http_body_view(void *req, const char **out_buf, size_t *out_len)
{
    struct mg_http_message *hm = req;

    if (hm == NULL || out_buf == NULL || out_len == NULL) {
        return -1;
    }

    *out_buf = hm->body.buf;
    *out_len = hm->body.len;
    return 0;
}

static int json_path(const char *key, char *out, size_t out_size)
{
    int n;

    if (key == NULL || key[0] == '\0' || out == NULL || out_size == 0) {
        return -1;
    }
    for (const char *p = key; *p != '\0'; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_') {
            return -1;
        }
    }

    n = snprintf(out, out_size, "$.%s", key);
    return n > 0 && (size_t)n < out_size ? 0 : -1;
}

int http_json_get_string_field(const char *body, size_t body_len, const char *key,
                               char *out, size_t out_size)
{
    struct mg_str json;
    char path[128];
    char *value;
    size_t len;

    if (out == NULL || out_size == 0) {
        return -1;
    }
    out[0] = '\0';

    if (json_object_view(body, body_len, &json) != 0 ||
        json_path(key, path, sizeof(path)) != 0) {
        return -1;
    }

    value = mg_json_get_str(json, path);
    if (value == NULL) {
        return -1;
    }

    len = strlen(value);
    if (len >= out_size) {
        mg_free(value);
        return -1;
    }
    memcpy(out, value, len + 1);
    mg_free(value);
    return 0;
}

static int parse_u32_token(struct mg_str token, uint32_t *out)
{
    uint32_t value = 0;

    if (out == NULL || token.buf == NULL || token.len == 0) {
        return -1;
    }

    for (size_t i = 0; i < token.len; i++) {
        unsigned char c = (unsigned char)token.buf[i];
        uint32_t digit;

        if (!isdigit(c)) {
            return -1;
        }
        digit = (uint32_t)(c - '0');
        if (value > (UINT32_MAX - digit) / 10) {
            return -1;
        }
        value = value * 10 + digit;
    }

    *out = value;
    return 0;
}

int http_json_get_u32_field(const char *body, size_t body_len, const char *key,
                            uint32_t *out)
{
    struct mg_str json;
    struct mg_str token;
    char path[128];

    if (json_object_view(body, body_len, &json) != 0 ||
        json_path(key, path, sizeof(path)) != 0) {
        return -1;
    }

    token = mg_json_get_tok(json, path);
    return parse_u32_token(token, out);
}

static int optional_token(const char *body, size_t body_len, const char *key,
                          struct mg_str *json, char *path, size_t path_size,
                          struct mg_str *token)
{
    if (json_object_view(body, body_len, json) != 0 ||
        json_path(key, path, path_size) != 0) {
        return HTTP_JSON_FIELD_INVALID;
    }
    *token = mg_json_get_tok(*json, path);
    if (token->buf == NULL) {
        return HTTP_JSON_FIELD_ABSENT;
    }
    if (token->len == 4 && memcmp(token->buf, "null", 4) == 0) {
        return HTTP_JSON_FIELD_NULL;
    }
    return HTTP_JSON_FIELD_VALUE;
}

int http_json_get_optional_string(const char *body, size_t body_len,
                                  const char *key, char *out,
                                  size_t out_size)
{
    struct mg_str json;
    struct mg_str token;
    char path[128];
    char *value;
    size_t len;
    int state;

    if (out == NULL || out_size == 0) {
        return HTTP_JSON_FIELD_INVALID;
    }
    out[0] = '\0';
    state = optional_token(body, body_len, key, &json, path, sizeof(path), &token);
    if (state != HTTP_JSON_FIELD_VALUE) {
        return state;
    }
    value = mg_json_get_str(json, path);
    if (value == NULL) {
        return HTTP_JSON_FIELD_INVALID;
    }
    len = strlen(value);
    if (len >= out_size) {
        mg_free(value);
        return HTTP_JSON_FIELD_INVALID;
    }
    memcpy(out, value, len + 1);
    mg_free(value);
    return HTTP_JSON_FIELD_VALUE;
}

int http_json_get_optional_u32(const char *body, size_t body_len,
                               const char *key, uint32_t *out)
{
    struct mg_str json;
    struct mg_str token;
    char path[128];
    int state;

    state = optional_token(body, body_len, key, &json, path, sizeof(path), &token);
    if (state != HTTP_JSON_FIELD_VALUE) {
        return state;
    }
    return parse_u32_token(token, out) == 0 ? HTTP_JSON_FIELD_VALUE
                                            : HTTP_JSON_FIELD_INVALID;
}

int http_json_get_u32_array(const char *body, size_t body_len, const char *key,
                            uint32_t *out, size_t max_out, size_t *out_count)
{
    struct mg_str json;
    struct mg_str array;
    struct mg_str value;
    char path[128];
    size_t offset = 0;
    size_t n = 0;

    if (out_count == NULL) {
        return -1;
    }
    *out_count = 0;

    if (json_object_view(body, body_len, &json) != 0 ||
        json_path(key, path, sizeof(path)) != 0) {
        return -1;
    }

    array = mg_json_get_tok(json, path);
    if (array.buf == NULL || array.len < 2 || array.buf[0] != '[' ||
        array.buf[array.len - 1] != ']') {
        return -1;
    }

    while ((offset = mg_json_next(array, offset, NULL, &value)) != 0) {
        if (n >= max_out || out == NULL) {
            return -1;
        }
        if (parse_u32_token(value, &out[n]) != 0) {
            return -1;
        }
        n++;
    }

    *out_count = n;
    return 0;
}

int http_req_json_get_string(void *req, const char *key, char *out, size_t out_size)
{
    const char *buf;
    size_t len;

    if (http_body_view(req, &buf, &len) != 0) {
        return -1;
    }
    return http_json_get_string_field(buf, len, key, out, out_size);
}

int http_req_json_get_u32(void *req, const char *key, uint32_t *out)
{
    const char *buf;
    size_t len;

    if (http_body_view(req, &buf, &len) != 0) {
        return -1;
    }
    return http_json_get_u32_field(buf, len, key, out);
}

int http_req_json_get_optional_string(void *req, const char *key, char *out,
                                      size_t out_size)
{
    const char *buf;
    size_t len;

    if (http_body_view(req, &buf, &len) != 0) {
        return HTTP_JSON_FIELD_INVALID;
    }
    return http_json_get_optional_string(buf, len, key, out, out_size);
}

int http_req_json_get_optional_u32(void *req, const char *key, uint32_t *out)
{
    const char *buf;
    size_t len;

    if (http_body_view(req, &buf, &len) != 0) {
        return HTTP_JSON_FIELD_INVALID;
    }
    return http_json_get_optional_u32(buf, len, key, out);
}

int http_req_json_get_u32_array(void *req, const char *key, uint32_t *out,
                                size_t max_out, size_t *out_count)
{
    const char *buf;
    size_t len;

    if (http_body_view(req, &buf, &len) != 0) {
        return -1;
    }
    return http_json_get_u32_array(buf, len, key, out, max_out, out_count);
}
