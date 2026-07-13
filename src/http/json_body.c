#include "media_server/http/json_body.h"

#include "mongoose.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *skip_ws(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
    }
    return p;
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

static int find_key(const char *body, size_t body_len, const char *key,
                    const char **value_start)
{
    char pattern[128];
    size_t key_len;
    const char *p;
    const char *end;

    if (body == NULL || key == NULL || value_start == NULL) {
        return -1;
    }

    key_len = strlen(key);
    if (key_len == 0 || key_len + 4 >= sizeof(pattern)) {
        return -1;
    }

    /* "key" */
    pattern[0] = '"';
    memcpy(pattern + 1, key, key_len);
    pattern[1 + key_len] = '"';
    pattern[2 + key_len] = '\0';

    end = body + body_len;
    p = body;
    while (p < end) {
        size_t rem = (size_t)(end - p);
        const char *hit;
        if (rem < key_len + 2) {
            break;
        }
        hit = NULL;
        for (size_t i = 0; i + key_len + 2 <= rem; i++) {
            if (p[i] == '"' && memcmp(p + i + 1, key, key_len) == 0 &&
                p[i + 1 + key_len] == '"') {
                hit = p + i;
                break;
            }
        }
        if (hit == NULL) {
            return -1;
        }

        p = hit + key_len + 2;
        p = skip_ws(p, end);
        if (p >= end || *p != ':') {
            continue;
        }
        p++;
        p = skip_ws(p, end);
        if (p >= end) {
            return -1;
        }
        *value_start = p;
        return 0;
    }
    return -1;
}

int http_json_get_string_field(const char *body, size_t body_len, const char *key,
                               char *out, size_t out_size)
{
    const char *p;
    const char *end;
    size_t n = 0;

    if (out == NULL || out_size == 0) {
        return -1;
    }
    out[0] = '\0';

    if (find_key(body, body_len, key, &p) != 0) {
        return -1;
    }

    end = body + body_len;
    if (*p != '"') {
        return -1;
    }
    p++;

    while (p < end && *p != '"') {
        char c = *p++;
        if (c == '\\' && p < end) {
            c = *p++;
            if (c == 'n') {
                c = '\n';
            } else if (c == 't') {
                c = '\t';
            } else if (c == 'r') {
                c = '\r';
            }
            /* otherwise keep escaped char as-is (", \, /) */
        }
        if (n + 1 >= out_size) {
            return -1;
        }
        out[n++] = c;
    }
    if (p >= end || *p != '"') {
        return -1;
    }
    out[n] = '\0';
    return 0;
}

int http_json_get_u32_field(const char *body, size_t body_len, const char *key,
                            uint32_t *out)
{
    const char *p;
    const char *end;
    char *stop = NULL;
    unsigned long long v;

    if (out == NULL) {
        return -1;
    }

    if (find_key(body, body_len, key, &p) != 0) {
        return -1;
    }

    end = body + body_len;
    if (p >= end || !isdigit((unsigned char)*p)) {
        return -1;
    }

    v = strtoull(p, &stop, 10);
    if (stop == p || v > UINT32_MAX) {
        return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

int http_json_get_u32_array(const char *body, size_t body_len, const char *key,
                            uint32_t *out, size_t max_out, size_t *out_count)
{
    const char *p;
    const char *end;
    size_t n = 0;

    if (out_count == NULL) {
        return -1;
    }
    *out_count = 0;

    if (find_key(body, body_len, key, &p) != 0) {
        return -1;
    }

    end = body + body_len;
    if (*p != '[') {
        return -1;
    }
    p++;
    p = skip_ws(p, end);

    if (p < end && *p == ']') {
        return 0;
    }

    while (p < end) {
        char *stop = NULL;
        unsigned long long v;

        p = skip_ws(p, end);
        if (p >= end || !isdigit((unsigned char)*p)) {
            return -1;
        }
        v = strtoull(p, &stop, 10);
        if (stop == p || v > UINT32_MAX) {
            return -1;
        }
        p = stop;
        if (n >= max_out || out == NULL) {
            return -1;
        }
        out[n++] = (uint32_t)v;

        p = skip_ws(p, end);
        if (p < end && *p == ',') {
            p++;
            continue;
        }
        if (p < end && *p == ']') {
            *out_count = n;
            return 0;
        }
        return -1;
    }
    return -1;
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
