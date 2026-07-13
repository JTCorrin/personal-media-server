#include "media_server/util/path.h"

#include <string.h>
#include <strings.h>

int path_join(char *out, size_t out_size, const char *dir, const char *name)
{
    size_t dir_len;
    size_t name_len;
    int need_slash;
    size_t total;
    size_t pos = 0;

    if (out == NULL || out_size == 0 || dir == NULL || name == NULL) {
        return -1;
    }

    /* Treat absolute name as already complete. */
    if (name[0] == '/') {
        if (strlen(name) + 1 > out_size) {
            return -1;
        }
        memcpy(out, name, strlen(name) + 1);
        return 0;
    }

    dir_len = strlen(dir);
    name_len = strlen(name);
    need_slash = (dir_len > 0 && dir[dir_len - 1] != '/');
    total = dir_len + (need_slash ? 1 : 0) + name_len + 1;

    if (total > out_size) {
        return -1;
    }

    if (dir_len > 0) {
        memcpy(out, dir, dir_len);
        pos = dir_len;
        if (need_slash) {
            out[pos++] = '/';
        }
    }

    memcpy(out + pos, name, name_len);
    out[pos + name_len] = '\0';
    return 0;
}

const char *path_basename(const char *path)
{
    const char *slash;

    if (path == NULL || path[0] == '\0') {
        return path;
    }

    slash = strrchr(path, '/');
    if (slash == NULL) {
        return path;
    }

    return slash + 1;
}

int path_dirname(char *out, size_t out_size, const char *path)
{
    const char *slash;
    size_t len;

    if (out == NULL || out_size == 0 || path == NULL) {
        return -1;
    }

    slash = strrchr(path, '/');
    if (slash == NULL) {
        out[0] = '\0';
        return 0;
    }

    len = (size_t)(slash - path);
    if (len + 1 > out_size) {
        return -1;
    }
    memcpy(out, path, len);
    out[len] = '\0';
    return 0;
}

const char *path_extension(const char *path)
{
    const char *base;
    const char *dot;

    if (path == NULL) {
        return NULL;
    }

    base = path_basename(path);
    if (base == NULL || base[0] == '\0') {
        return NULL;
    }

    /* Skip leading dot on hidden files like ".gitignore" with no real ext. */
    dot = strrchr(base, '.');
    if (dot == NULL || dot == base || dot[1] == '\0') {
        return NULL;
    }

    return dot;
}

bool path_has_extension(const char *path, const char *ext)
{
    const char *path_ext;
    const char *want;

    if (ext == NULL || ext[0] == '\0') {
        return false;
    }

    path_ext = path_extension(path);
    if (path_ext == NULL) {
        return false;
    }

    want = (ext[0] == '.') ? ext : NULL;
    if (want != NULL) {
        return strcasecmp(path_ext, want) == 0;
    }

    /* Compare without requiring caller to pass the leading dot. */
    return strcasecmp(path_ext + 1, ext) == 0;
}
