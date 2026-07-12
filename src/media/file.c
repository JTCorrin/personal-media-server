#include "media_server/media/file.h"

#include "media_server/util/path.h"

#include <stdbool.h>
#include <string.h>

const char *media_content_type(media_kind_t kind, const char *path)
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

    if (kind == MEDIA_KIND_AUDIO) {
        return "application/octet-stream";
    }
    if (kind == MEDIA_KIND_IMAGE) {
        return "application/octet-stream";
    }

    return "application/octet-stream";
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
