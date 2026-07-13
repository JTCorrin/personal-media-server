#ifndef MEDIA_SERVER_LIBRARY_SEARCH_H
#define MEDIA_SERVER_LIBRARY_SEARCH_H

#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"

#include <stdbool.h>

/*
 * Relevance score for ranking (higher is better; 0 = no match).
 * Prefer exact/prefix/substring over fuzzy-only hits.
 */
int search_track_score(const catalog_item_t *item, const char *q, bool fuzzy);
int search_artist_score(const browse_artist_t *artist, const char *q, bool fuzzy);
int search_album_score(const browse_album_t *album, const char *q, bool fuzzy);

/* Audio item matches q against title/artist/album/filename/path (case-insensitive). */
bool search_track_matches(const catalog_item_t *item, const char *q);
bool search_track_matches_ex(const catalog_item_t *item, const char *q, bool fuzzy);

bool search_artist_matches(const browse_artist_t *artist, const char *q);
bool search_artist_matches_ex(const browse_artist_t *artist, const char *q, bool fuzzy);

bool search_album_matches(const browse_album_t *album, const char *q);
bool search_album_matches_ex(const browse_album_t *album, const char *q, bool fuzzy);

#endif /* MEDIA_SERVER_LIBRARY_SEARCH_H */
