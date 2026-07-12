#include "media_server/util/string_buf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Grow until at least `needed` bytes beyond the current length fit. */
static int ensure_room(string_buf_t *sb, size_t needed)
{
    size_t required;
    size_t new_cap;
    char *grown;

    if (sb == NULL || sb->data == NULL) {
        return -1;
    }

    required = sb->len + needed + 1; /* +1 for the terminator */
    if (required < sb->len) {
        return -1; /* size_t overflow */
    }

    if (required <= sb->cap) {
        return 0;
    }

    new_cap = sb->cap;
    while (new_cap < required) {
        if (new_cap > (size_t)-1 / 2) {
            return -1;
        }
        new_cap *= 2;
    }

    grown = realloc(sb->data, new_cap);
    if (grown == NULL) {
        return -1;
    }

    sb->data = grown;
    sb->cap = new_cap;
    return 0;
}

int string_buf_init(string_buf_t *sb, size_t initial_cap)
{
    if (sb == NULL) {
        return -1;
    }

    if (initial_cap < 16) {
        initial_cap = 16;
    }

    sb->data = malloc(initial_cap);
    if (sb->data == NULL) {
        sb->len = 0;
        sb->cap = 0;
        return -1;
    }

    sb->data[0] = '\0';
    sb->len = 0;
    sb->cap = initial_cap;
    return 0;
}

void string_buf_free(string_buf_t *sb)
{
    if (sb == NULL) {
        return;
    }

    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

int string_buf_append_n(string_buf_t *sb, const char *s, size_t n)
{
    if (s == NULL) {
        return -1;
    }

    if (ensure_room(sb, n) != 0) {
        return -1;
    }

    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
    return 0;
}

int string_buf_append(string_buf_t *sb, const char *s)
{
    if (s == NULL) {
        return -1;
    }
    return string_buf_append_n(sb, s, strlen(s));
}

int string_buf_append_char(string_buf_t *sb, char c)
{
    return string_buf_append_n(sb, &c, 1);
}

int string_buf_append_fmt(string_buf_t *sb, const char *fmt, ...)
{
    va_list args;
    int needed;

    if (sb == NULL || sb->data == NULL || fmt == NULL) {
        return -1;
    }

    /* First pass measures, second pass writes into the grown buffer. */
    va_start(args, fmt);
    needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (needed < 0) {
        return -1;
    }

    if (ensure_room(sb, (size_t)needed) != 0) {
        return -1;
    }

    va_start(args, fmt);
    vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, args);
    va_end(args);

    sb->len += (size_t)needed;
    return 0;
}

int string_buf_append_json_string(string_buf_t *sb, const char *s)
{
    if (s == NULL) {
        return -1;
    }

    if (string_buf_append_char(sb, '"') != 0) {
        return -1;
    }

    for (const char *p = s; *p != '\0'; p++) {
        unsigned char c = (unsigned char)*p;
        int rc;

        switch (c) {
        case '"':
            rc = string_buf_append(sb, "\\\"");
            break;
        case '\\':
            rc = string_buf_append(sb, "\\\\");
            break;
        case '\b':
            rc = string_buf_append(sb, "\\b");
            break;
        case '\f':
            rc = string_buf_append(sb, "\\f");
            break;
        case '\n':
            rc = string_buf_append(sb, "\\n");
            break;
        case '\r':
            rc = string_buf_append(sb, "\\r");
            break;
        case '\t':
            rc = string_buf_append(sb, "\\t");
            break;
        default:
            if (c < 0x20) {
                rc = string_buf_append_fmt(sb, "\\u%04x", c);
            } else {
                rc = string_buf_append_char(sb, (char)c);
            }
            break;
        }

        if (rc != 0) {
            return -1;
        }
    }

    return string_buf_append_char(sb, '"');
}

const char *string_buf_cstr(const string_buf_t *sb)
{
    if (sb == NULL || sb->data == NULL) {
        return "";
    }
    return sb->data;
}
