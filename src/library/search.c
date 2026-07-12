#include "media_server/library/search.h"

#include "media_server/media/kind.h"
#include "media_server/util/str.h"

bool search_track_matches(const catalog_item_t *item, const char *q)
{
    if (item == NULL || q == NULL || q[0] == '\0' || item->kind != MEDIA_KIND_AUDIO) {
        return false;
    }

    return str_contains_ci(item->title, q) || str_contains_ci(item->artist, q) ||
           str_contains_ci(item->album, q) || str_contains_ci(item->filename, q) ||
           str_contains_ci(item->path, q);
}

bool search_artist_matches(const browse_artist_t *artist, const char *q)
{
    if (artist == NULL || q == NULL || q[0] == '\0') {
        return false;
    }
    return str_contains_ci(artist->name, q);
}

bool search_album_matches(const browse_album_t *album, const char *q)
{
    if (album == NULL || q == NULL || q[0] == '\0') {
        return false;
    }
    return str_contains_ci(album->name, q) || str_contains_ci(album->artist, q);
}
