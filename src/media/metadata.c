#include "media_server/media/metadata.h"

#include "media_server/media/path_meta.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <taglib/tag_c.h>

static void copy_trimmed(char *dst, size_t dst_size, const char *src,
                         size_t src_len)
{
    size_t start = 0;
    size_t n;

    if (dst == NULL || dst_size == 0) {
        return;
    }
    while (start < src_len && isspace((unsigned char)src[start])) {
        start++;
    }
    while (src_len > start && isspace((unsigned char)src[src_len - 1])) {
        src_len--;
    }
    n = src_len - start;
    if (n >= dst_size) {
        n = dst_size - 1;
    }
    if (n > 0) {
        memmove(dst, src + start, n);
    }
    dst[n] = '\0';
}

static int parse_number_segment(const char *start, size_t len, uint32_t *out)
{
    uint32_t value = 0;
    size_t i = 0;

    while (i < len && isspace((unsigned char)start[i])) {
        i++;
    }
    if (i == len || !isdigit((unsigned char)start[i])) {
        return 0;
    }
    for (; i < len && isdigit((unsigned char)start[i]); i++) {
        uint32_t digit = (uint32_t)(start[i] - '0');
        if (value > (UINT32_MAX - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
    }
    while (i < len && isspace((unsigned char)start[i])) {
        i++;
    }
    if (i != len || value == 0) {
        return 0;
    }
    *out = value;
    return 1;
}

static int parse_full_filename(media_metadata_t *meta, const char *stem)
{
    const char *d1 = strstr(stem, " - ");
    const char *d2;
    const char *d3;
    uint32_t number;

    if (d1 == NULL) {
        return 0;
    }
    d2 = strstr(d1 + 3, " - ");
    if (d2 == NULL) {
        return 0;
    }
    d3 = strstr(d2 + 3, " - ");
    if (d3 == NULL ||
        !parse_number_segment(d2 + 3, (size_t)(d3 - (d2 + 3)), &number)) {
        return 0;
    }
    copy_trimmed(meta->title, sizeof(meta->title), d3 + 3, strlen(d3 + 3));
    if (meta->title[0] == '\0') {
        return 0;
    }
    meta->track_number = number;
    return 1;
}

static int parse_leading_track(media_metadata_t *meta, const char *stem)
{
    const char *p = stem;
    const char *title;
    uint32_t number = 0;

    if (!isdigit((unsigned char)*p)) {
        return 0;
    }
    while (isdigit((unsigned char)*p)) {
        uint32_t digit = (uint32_t)(*p - '0');
        if (number > (UINT32_MAX - digit) / 10) {
            return 0;
        }
        number = number * 10 + digit;
        p++;
    }
    if (number == 0) {
        return 0;
    }
    if (strncmp(p, " - ", 3) == 0) {
        title = p + 3;
    } else if (*p == '.' && isspace((unsigned char)p[1])) {
        title = p + 2;
    } else if (isspace((unsigned char)*p)) {
        title = p;
        while (isspace((unsigned char)*title)) {
            title++;
        }
    } else {
        return 0;
    }
    if (*title == '\0') {
        return 0;
    }
    copy_trimmed(meta->title, sizeof(meta->title), title, strlen(title));
    meta->track_number = number;
    return 1;
}

int media_metadata_from_path(const char *rel_path, media_metadata_t *out)
{
    if (rel_path == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (media_path_meta(rel_path, out->artist, sizeof(out->artist), out->album,
                        sizeof(out->album), out->title, sizeof(out->title)) != 0) {
        return -1;
    }
    if (!parse_full_filename(out, out->title)) {
        (void)parse_leading_track(out, out->title);
    }
    return 0;
}

static int copy_property(TagLib_File *file, const char *name, char *dst,
                         size_t dst_size)
{
    char **values = taglib_property_get(file, name);
    int copied = 0;

    if (values != NULL && values[0] != NULL && values[0][0] != '\0') {
        copy_trimmed(dst, dst_size, values[0], strlen(values[0]));
        copied = dst[0] != '\0';
    }
    if (values != NULL) {
        taglib_property_free(values);
    }
    return copied;
}

static uint32_t number_property(TagLib_File *file, const char *name)
{
    char **values = taglib_property_get(file, name);
    uint32_t number = 0;

    if (values != NULL && values[0] != NULL) {
        const char *p = values[0];
        while (isspace((unsigned char)*p)) {
            p++;
        }
        while (isdigit((unsigned char)*p)) {
            uint32_t digit = (uint32_t)(*p - '0');
            if (number > (UINT32_MAX - digit) / 10) {
                number = 0;
                break;
            }
            number = number * 10 + digit;
            p++;
        }
    }
    if (values != NULL) {
        taglib_property_free(values);
    }
    return number;
}

int media_metadata_read(const char *abs_path, const char *rel_path,
                        media_metadata_t *out)
{
    TagLib_File *file;

    if (abs_path == NULL || media_metadata_from_path(rel_path, out) != 0) {
        return -1;
    }
    file = taglib_file_new(abs_path);
    if (file == NULL) {
        return 0;
    }
    if (!taglib_file_is_valid(file)) {
        taglib_file_free(file);
        return 0;
    }

    (void)copy_property(file, "TITLE", out->title, sizeof(out->title));
    (void)copy_property(file, "ARTIST", out->artist, sizeof(out->artist));
    (void)copy_property(file, "ALBUM", out->album, sizeof(out->album));
    (void)copy_property(file, "DATE", out->release_date, sizeof(out->release_date));
    (void)copy_property(file, "GENRE", out->genre, sizeof(out->genre));
    {
        uint32_t value = number_property(file, "TRACKNUMBER");
        if (value != 0) {
            out->track_number = value;
        }
        value = number_property(file, "DISCNUMBER");
        if (value != 0) {
            out->disc_number = value;
        }
    }
    taglib_file_free(file);
    return 0;
}
