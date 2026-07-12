#ifndef MEDIA_SERVER_UTIL_STRING_BUF_H
#define MEDIA_SERVER_UTIL_STRING_BUF_H

#include <stdarg.h>
#include <stddef.h>

/*
 * Growable, always-NUL-terminated string buffer.
 *
 * Unlike the opaque types elsewhere in this codebase, the struct is public so
 * callers can stack-allocate one; treat the fields as read-only and use the
 * functions below. All append functions return 0 on success and -1 on
 * allocation failure; on failure the buffer keeps its previous contents and
 * must still be released with string_buf_free().
 */
typedef struct string_buf {
    char *data;   /* NUL-terminated; never NULL after string_buf_init */
    size_t len;   /* strlen(data) */
    size_t cap;   /* allocated bytes, always > len */
} string_buf_t;

int string_buf_init(string_buf_t *sb, size_t initial_cap);
void string_buf_free(string_buf_t *sb);

int string_buf_append(string_buf_t *sb, const char *s);
int string_buf_append_n(string_buf_t *sb, const char *s, size_t n);
int string_buf_append_char(string_buf_t *sb, char c);

int string_buf_append_fmt(string_buf_t *sb, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/*
 * Append s as a JSON string literal, including the surrounding double quotes.
 * Escapes '"', '\\', and all control characters (RFC 8259 section 7).
 */
int string_buf_append_json_string(string_buf_t *sb, const char *s);

/* Convenience accessor; returns "" for a NULL buffer. */
const char *string_buf_cstr(const string_buf_t *sb);

#endif /* MEDIA_SERVER_UTIL_STRING_BUF_H */
