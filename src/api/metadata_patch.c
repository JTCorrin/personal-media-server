#include "media_server/api/metadata_patch.h"

#include "media_server/http/json_body.h"

#include <ctype.h>
#include <string.h>

static int valid_date(const char *s)
{
    size_t n = strlen(s);
    int year;

    if (n == 0) {
        return 1; /* explicit blank is a valid override */
    }
    if (n != 4 && n != 7 && n != 10) {
        return 0;
    }
    for (size_t i = 0; i < n; i++) {
        if ((i == 4 || i == 7) ? s[i] != '-' : !isdigit((unsigned char)s[i])) {
            return 0;
        }
    }
    year = (s[0] - '0') * 1000 + (s[1] - '0') * 100 +
           (s[2] - '0') * 10 + (s[3] - '0');
    if (year == 0) {
        return 0;
    }
    if (n >= 7) {
        int month = (s[5] - '0') * 10 + (s[6] - '0');
        if (month < 1 || month > 12) {
            return 0;
        }
    }
    if (n == 10) {
        static const int days_per_month[] = {
            31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
        };
        int month = (s[5] - '0') * 10 + (s[6] - '0');
        int day = (s[8] - '0') * 10 + (s[9] - '0');
        int max_day = days_per_month[month - 1];
        if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0))) {
            max_day = 29;
        }
        if (day < 1 || day > max_day) {
            return 0;
        }
    }
    return 1;
}

static int parse_text(void *req, const char *key, uint32_t bit, char *dst,
                      size_t dst_size, metadata_patch_t *out, int nonempty,
                      int date)
{
    int state = http_req_json_get_optional_string(req, key, dst, dst_size);

    if (state == HTTP_JSON_FIELD_INVALID) {
        return -1;
    }
    if (state == HTTP_JSON_FIELD_NULL) {
        out->clear_mask |= bit;
    } else if (state == HTTP_JSON_FIELD_VALUE) {
        if ((nonempty && dst[0] == '\0') || (date && !valid_date(dst))) {
            return -1;
        }
        out->set_mask |= bit;
    }
    return 0;
}

static int parse_u32(void *req, const char *key, uint32_t bit, uint32_t *dst,
                     metadata_patch_t *out)
{
    int state = http_req_json_get_optional_u32(req, key, dst);

    if (state == HTTP_JSON_FIELD_INVALID) {
        return -1;
    }
    if (state == HTTP_JSON_FIELD_NULL) {
        out->clear_mask |= bit;
    } else if (state == HTTP_JSON_FIELD_VALUE) {
        if (*dst == 0 || *dst > 65535) {
            return -1;
        }
        out->set_mask |= bit;
    }
    return 0;
}

int api_parse_track_metadata_patch(void *req, metadata_patch_t *out)
{
    if (req == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (parse_text(req, "title", METADATA_FIELD_TITLE, out->values.title,
                   sizeof(out->values.title), out, 1, 0) != 0 ||
        parse_text(req, "artist", METADATA_FIELD_ARTIST, out->values.artist,
                   sizeof(out->values.artist), out, 0, 0) != 0 ||
        parse_text(req, "album", METADATA_FIELD_ALBUM, out->values.album,
                   sizeof(out->values.album), out, 0, 0) != 0 ||
        parse_text(req, "release_date", METADATA_FIELD_RELEASE_DATE,
                   out->values.release_date, sizeof(out->values.release_date), out,
                   0, 1) != 0 ||
        parse_text(req, "genre", METADATA_FIELD_GENRE, out->values.genre,
                   sizeof(out->values.genre), out, 0, 0) != 0 ||
        parse_u32(req, "track_number", METADATA_FIELD_TRACK_NUMBER,
                  &out->values.track_number, out) != 0 ||
        parse_u32(req, "disc_number", METADATA_FIELD_DISC_NUMBER,
                  &out->values.disc_number, out) != 0) {
        return -1;
    }
    return (out->set_mask | out->clear_mask) != 0 ? 0 : -1;
}

int api_parse_album_metadata_patch(void *req, metadata_patch_t *out)
{
    if (req == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (parse_text(req, "name", METADATA_FIELD_ALBUM, out->values.album,
                   sizeof(out->values.album), out, 0, 0) != 0 ||
        parse_text(req, "artist", METADATA_FIELD_ARTIST, out->values.artist,
                   sizeof(out->values.artist), out, 0, 0) != 0 ||
        parse_text(req, "release_date", METADATA_FIELD_RELEASE_DATE,
                   out->values.release_date, sizeof(out->values.release_date), out,
                   0, 1) != 0 ||
        parse_text(req, "genre", METADATA_FIELD_GENRE, out->values.genre,
                   sizeof(out->values.genre), out, 0, 0) != 0) {
        return -1;
    }
    return (out->set_mask | out->clear_mask) != 0 ? 0 : -1;
}
