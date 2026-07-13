#include "media_server/api/browse_json.h"

int append_artist_json(string_buf_t *sb, const browse_artist_t *artist)
{
    if (sb == NULL || artist == NULL) {
        return -1;
    }
    if (string_buf_append_fmt(sb, "{\"id\":%u,\"name\":", artist->id) != 0) {
        return -1;
    }
    if (string_buf_append_json_string(sb, artist->name) != 0) {
        return -1;
    }
    return string_buf_append_fmt(sb, ",\"album_count\":%zu,\"track_count\":%zu}",
                                 artist->album_count, artist->track_count);
}

int append_album_json(string_buf_t *sb, const browse_album_t *album)
{
    if (sb == NULL || album == NULL) {
        return -1;
    }
    if (string_buf_append_fmt(sb, "{\"id\":%u,\"name\":", album->id) != 0) {
        return -1;
    }
    if (string_buf_append_json_string(sb, album->name) != 0) {
        return -1;
    }
    if (string_buf_append(sb, ",\"artist\":") != 0) {
        return -1;
    }
    if (string_buf_append_json_string(sb, album->artist) != 0) {
        return -1;
    }
    if (string_buf_append_fmt(sb, ",\"artist_id\":%u,\"track_count\":%zu,",
                              album->artist_id, album->track_count) != 0) {
        return -1;
    }
    if (album->cover_id == 0) {
        return string_buf_append(sb, "\"cover_id\":null}");
    }
    return string_buf_append_fmt(sb, "\"cover_id\":%u}", album->cover_id);
}
