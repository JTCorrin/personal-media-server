#include "media_server/library/search.h"

#include "media_server/media/kind.h"
#include "media_server/util/str.h"

static int max_score(int a, int b)
{
    return a > b ? a : b;
}

static int field_score(const char *field, const char *q, bool fuzzy)
{
    if (field == NULL) {
        return 0;
    }
    return str_match_score_ci(field, q, fuzzy);
}

int search_track_score(const catalog_item_t *item, const char *q, bool fuzzy)
{
    int best;

    if (item == NULL || q == NULL || q[0] == '\0' || item->kind != MEDIA_KIND_AUDIO) {
        return 0;
    }

    best = field_score(item->title, q, fuzzy);
    best = max_score(best, field_score(item->artist, q, fuzzy));
    best = max_score(best, field_score(item->album, q, fuzzy));
    best = max_score(best, field_score(item->filename, q, fuzzy));
    best = max_score(best, field_score(item->path, q, fuzzy));
    return best;
}

int search_artist_score(const browse_artist_t *artist, const char *q, bool fuzzy)
{
    if (artist == NULL || q == NULL || q[0] == '\0') {
        return 0;
    }
    return field_score(artist->name, q, fuzzy);
}

int search_album_score(const browse_album_t *album, const char *q, bool fuzzy)
{
    int best;

    if (album == NULL || q == NULL || q[0] == '\0') {
        return 0;
    }
    best = field_score(album->name, q, fuzzy);
    return max_score(best, field_score(album->artist, q, fuzzy));
}

bool search_track_matches_ex(const catalog_item_t *item, const char *q, bool fuzzy)
{
    return search_track_score(item, q, fuzzy) > 0;
}

bool search_track_matches(const catalog_item_t *item, const char *q)
{
    return search_track_matches_ex(item, q, false);
}

bool search_artist_matches_ex(const browse_artist_t *artist, const char *q, bool fuzzy)
{
    return search_artist_score(artist, q, fuzzy) > 0;
}

bool search_artist_matches(const browse_artist_t *artist, const char *q)
{
    return search_artist_matches_ex(artist, q, false);
}

bool search_album_matches_ex(const browse_album_t *album, const char *q, bool fuzzy)
{
    return search_album_score(album, q, fuzzy) > 0;
}

bool search_album_matches(const browse_album_t *album, const char *q)
{
    return search_album_matches_ex(album, q, false);
}
