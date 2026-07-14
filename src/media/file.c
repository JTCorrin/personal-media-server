#include "media_server/media/file.h"

#include "media_server/util/path.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>

const char *media_content_type(const char *path)
{
    if (path_has_extension(path, "mp3")) {
        return "audio/mpeg";
    }
    if (path_has_extension(path, "flac")) {
        return "audio/flac";
    }
    if (path_has_extension(path, "ogg")) {
        return "audio/ogg";
    }
    if (path_has_extension(path, "m4a")) {
        return "audio/mp4";
    }
    if (path_has_extension(path, "wav")) {
        return "audio/wav";
    }
    if (path_has_extension(path, "jpg") || path_has_extension(path, "jpeg")) {
        return "image/jpeg";
    }
    if (path_has_extension(path, "png")) {
        return "image/png";
    }
    if (path_has_extension(path, "webp")) {
        return "image/webp";
    }

    return "application/octet-stream";
}

int media_cover_ext_from_content_type(const char *content_type, char *ext_out,
                                      size_t ext_out_size)
{
    const char *p;
    const char *end;
    size_t len;
    char type[64];
    const char *ext;

    if (content_type == NULL || ext_out == NULL || ext_out_size < 5) {
        return -1;
    }

    p = content_type;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    end = p;
    while (*end != '\0' && *end != ';') {
        end++;
    }
    while (end > p && isspace((unsigned char)end[-1])) {
        end--;
    }
    len = (size_t)(end - p);
    if (len == 0 || len >= sizeof(type)) {
        return -1;
    }
    for (size_t i = 0; i < len; i++) {
        type[i] = (char)tolower((unsigned char)p[i]);
    }
    type[len] = '\0';

    if (strcmp(type, "image/jpeg") == 0) {
        ext = "jpg";
    } else if (strcmp(type, "image/png") == 0) {
        ext = "png";
    } else if (strcmp(type, "image/webp") == 0) {
        ext = "webp";
    } else {
        return -1;
    }

    if (strlen(ext) + 1 > ext_out_size) {
        return -1;
    }
    memcpy(ext_out, ext, strlen(ext) + 1);
    return 0;
}

static bool path_has_dotdot(const char *rel_path)
{
    const char *p = rel_path;

    while (*p != '\0') {
        const char *slash = strchr(p, '/');
        size_t len = slash == NULL ? strlen(p) : (size_t)(slash - p);

        if (len == 2 && p[0] == '.' && p[1] == '.') {
            return true;
        }

        if (slash == NULL) {
            break;
        }
        p = slash + 1;
    }

    return false;
}

int media_resolve_path(const char *library_root, const char *rel_path, char *out,
                       size_t out_size)
{
    if (library_root == NULL || library_root[0] == '\0' || rel_path == NULL ||
        rel_path[0] == '\0' || out == NULL || out_size == 0) {
        return -1;
    }

    if (rel_path[0] == '/') {
        return -1;
    }

    if (path_has_dotdot(rel_path)) {
        return -1;
    }

    return path_join(out, out_size, library_root, rel_path);
}
