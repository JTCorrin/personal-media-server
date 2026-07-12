#ifndef MEDIA_SERVER_LIBRARY_SEARCH_H
#define MEDIA_SERVER_LIBRARY_SEARCH_H

#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"

#include <stdbool.h>

/* Audio item matches q against title/artist/album/filename/path (case-insensitive). */
bool search_track_matches(const catalog_item_t *item, const char *q);

bool search_artist_matches(const browse_artist_t *artist, const char *q);
bool search_album_matches(const browse_album_t *album, const char *q);

#endif /* MEDIA_SERVER_LIBRARY_SEARCH_H */
