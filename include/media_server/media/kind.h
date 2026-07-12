#ifndef MEDIA_SERVER_MEDIA_KIND_H
#define MEDIA_SERVER_MEDIA_KIND_H

typedef enum media_kind {
    MEDIA_KIND_NONE = 0, /* not a recognized media file */
    MEDIA_KIND_AUDIO = 1,
    MEDIA_KIND_IMAGE,
    /* MEDIA_KIND_VIDEO later */
} media_kind_t;

/* Classify a path by extension. Returns MEDIA_KIND_NONE when not media. */
media_kind_t media_kind_from_path(const char *path);

/* Stable lowercase name for JSON: "audio", "image", or "none". */
const char *media_kind_name(media_kind_t kind);

#endif /* MEDIA_SERVER_MEDIA_KIND_H */
