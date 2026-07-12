#include "media_server/media/path_meta.h"

#include "media_server/util/path.h"

#include <string.h>

static void copy_trunc(char *dst, size_t dst_sz, const char *src, size_t src_len)
{
    size_t n;

    if (dst_sz == 0) {
        return;
    }

    n = src_len;
    if (n >= dst_sz) {
        n = dst_sz - 1;
    }

    if (n > 0) {
        memcpy(dst, src, n);
    }
    dst[n] = '\0';
}

int media_path_meta(const char *rel_path, char *artist, size_t artist_sz,
                    char *album, size_t album_sz, char *title, size_t title_sz)
{
    const char *seg0;
    const char *slash1;
    const char *seg1;
    const char *slash2;
    const char *base;
    const char *ext;
    size_t title_len;

    if (rel_path == NULL || rel_path[0] == '\0' || artist == NULL ||
        artist_sz == 0 || album == NULL || album_sz == 0 || title == NULL ||
        title_sz == 0) {
        return -1;
    }

    artist[0] = '\0';
    album[0] = '\0';
    title[0] = '\0';

    seg0 = rel_path;
    slash1 = strchr(seg0, '/');

    if (slash1 == NULL) {
        /* loose.mp3 */
        base = path_basename(rel_path);
    } else {
        copy_trunc(artist, artist_sz, seg0, (size_t)(slash1 - seg0));

        seg1 = slash1 + 1;
        slash2 = strchr(seg1, '/');

        if (slash2 == NULL) {
            /* Artist/song.mp3 */
            base = path_basename(rel_path);
        } else {
            /* Artist/Album/.../file */
            copy_trunc(album, album_sz, seg1, (size_t)(slash2 - seg1));
            base = path_basename(rel_path);
        }
    }

    if (base == NULL || base[0] == '\0') {
        return -1;
    }

    ext = path_extension(rel_path);
    if (ext != NULL && ext > base) {
        title_len = (size_t)(ext - base);
    } else {
        title_len = strlen(base);
    }

    copy_trunc(title, title_sz, base, title_len);
    return 0;
}
