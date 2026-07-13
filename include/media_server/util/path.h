#ifndef MEDIA_SERVER_UTIL_PATH_H
#define MEDIA_SERVER_UTIL_PATH_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Small path helpers for library scanning.
 * Paths use '/' as the separator (POSIX). No allocation — caller owns buffers.
 */

/*
 * Join directory and name into out.
 * Inserts '/' only when needed. Returns 0 on success, -1 if args invalid or
 * the result would not fit (including NUL) in out_size.
 */
int path_join(char *out, size_t out_size, const char *dir, const char *name);

/* Pointer to the final path component, or path itself if no slash. */
const char *path_basename(const char *path);

/*
 * Copy the parent directory of path into out (no trailing slash).
 * If path has no '/', writes "" and returns 0.
 * Returns 0 on success, -1 on bad args or overflow.
 */
int path_dirname(char *out, size_t out_size, const char *path);

/*
 * Pointer to the extension including the dot (e.g. ".mp3"), or NULL if none.
 * Only looks at the basename (dots in directories are ignored).
 */
const char *path_extension(const char *path);

/* Case-insensitive extension check. ext may be "mp3" or ".mp3". */
bool path_has_extension(const char *path, const char *ext);

#endif /* MEDIA_SERVER_UTIL_PATH_H */
