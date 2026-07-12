#include "media_server/media/kind.h"

#include "media_server/util/path.h"

#include <stdbool.h>
#include <stddef.h>

static const char *const AUDIO_EXTS[] = {
    "mp3", "flac", "ogg", "m4a", "wav", NULL,
};

static const char *const IMAGE_EXTS[] = {
    "jpg", "jpeg", "png", "webp", NULL,
};

static bool matches_any(const char *path, const char *const *exts)
{
    for (size_t i = 0; exts[i] != NULL; i++) {
        if (path_has_extension(path, exts[i])) {
            return true;
        }
    }
    return false;
}

media_kind_t media_kind_from_path(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return MEDIA_KIND_NONE;
    }

    if (matches_any(path, AUDIO_EXTS)) {
        return MEDIA_KIND_AUDIO;
    }
    if (matches_any(path, IMAGE_EXTS)) {
        return MEDIA_KIND_IMAGE;
    }

    return MEDIA_KIND_NONE;
}

const char *media_kind_name(media_kind_t kind)
{
    switch (kind) {
    case MEDIA_KIND_AUDIO:
        return "audio";
    case MEDIA_KIND_IMAGE:
        return "image";
    case MEDIA_KIND_NONE:
    default:
        return "none";
    }
}
