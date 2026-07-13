#ifndef MEDIA_SERVER_LIBRARY_USER_STORE_H
#define MEDIA_SERVER_LIBRARY_USER_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define USER_STORE_NAME_MAX 256
#define USER_STORE_SCHEMA_VERSION 1

typedef struct user_playlist {
    uint32_t id;
    char name[USER_STORE_NAME_MAX];
    time_t created_unix;
    time_t updated_unix;
    size_t track_count;
} user_playlist_t;

typedef struct user_history_entry {
    uint32_t track_id;
    time_t played_unix;
} user_history_entry_t;

typedef struct user_store user_store_t;

/* Open/create DB and ensure schema. Returns NULL on failure. */
user_store_t *user_store_open(const char *db_path);
void user_store_close(user_store_t *store);

/* Playlists */
int user_store_playlist_create(user_store_t *store, const char *name,
                               user_playlist_t *out);
int user_store_playlist_get(user_store_t *store, uint32_t id, user_playlist_t *out);
int user_store_playlist_rename(user_store_t *store, uint32_t id, const char *name);
int user_store_playlist_delete(user_store_t *store, uint32_t id);
size_t user_store_playlist_count(user_store_t *store);
/* List into out[0..max); returns number written. offset skips. */
size_t user_store_playlist_list(user_store_t *store, size_t offset, size_t limit,
                                user_playlist_t *out, size_t max);

/* Replace track list (ordered). track_ids may be NULL if count==0. */
int user_store_playlist_set_tracks(user_store_t *store, uint32_t playlist_id,
                                   const uint32_t *track_ids, size_t count);
/* Returns number of track ids written (may be < max). */
size_t user_store_playlist_get_tracks(user_store_t *store, uint32_t playlist_id,
                                      size_t offset, size_t limit, uint32_t *out,
                                      size_t max);
size_t user_store_playlist_track_count(user_store_t *store, uint32_t playlist_id);

/* Favourites */
int user_store_favourite_add(user_store_t *store, uint32_t track_id);
int user_store_favourite_remove(user_store_t *store, uint32_t track_id);
bool user_store_favourite_has(user_store_t *store, uint32_t track_id);
size_t user_store_favourite_count(user_store_t *store);
size_t user_store_favourite_list(user_store_t *store, size_t offset, size_t limit,
                                 uint32_t *out, size_t max);

/* History (append-only raw events, newest first when listing) */
int user_store_history_append(user_store_t *store, uint32_t track_id);
size_t user_store_history_count(user_store_t *store);
size_t user_store_history_list(user_store_t *store, size_t offset, size_t limit,
                               user_history_entry_t *out, size_t max);

#endif /* MEDIA_SERVER_LIBRARY_USER_STORE_H */
