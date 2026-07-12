#ifndef MEDIA_SERVER_MEDIA_PATH_META_H
#define MEDIA_SERVER_MEDIA_PATH_META_H

#include <stddef.h>

/*
 * Derive artist / album / title from a library-relative path.
 *
 * Rules (Navidrome-style folders):
 *   Artist/Album/track.mp3  -> artist, album, title (basename without extension)
 *   Artist/song.mp3         -> artist, "", title
 *   loose.mp3               -> "", "", title
 *   A/B/C/x.mp3             -> A, B, x  (deeper folders ignored for album)
 *
 * Writes NUL-terminated strings into the caller buffers (truncated if needed).
 * Returns 0 on success, -1 on invalid args (NULL/empty path or zero-size outs).
 */
int media_path_meta(const char *rel_path, char *artist, size_t artist_sz,
                    char *album, size_t album_sz, char *title, size_t title_sz);

#endif /* MEDIA_SERVER_MEDIA_PATH_META_H */
