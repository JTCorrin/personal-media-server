#include "unity.h"

#include "media_server/library/user_store.h"

#include <stdio.h>
#include <unistd.h>

static char db_path[64];
static user_store_t *store;

void setUp(void)
{
    snprintf(db_path, sizeof(db_path), "/tmp/ms_user_test_%d.db", (int)getpid());
    unlink(db_path);
    store = user_store_open(db_path);
    TEST_ASSERT_NOT_NULL(store);
}

void tearDown(void)
{
    user_store_close(store);
    store = NULL;
    unlink(db_path);
}

void test_playlist_create_and_tracks(void)
{
    user_playlist_t pl;
    uint32_t ids[] = {10, 20, 30};
    uint32_t out[8];
    size_t n;

    TEST_ASSERT_EQUAL_INT(0, user_store_playlist_create(store, "Mix", &pl));
    TEST_ASSERT_EQUAL_STRING("Mix", pl.name);
    TEST_ASSERT_TRUE(pl.id >= 1);
    TEST_ASSERT_EQUAL_UINT(0, pl.track_count);

    TEST_ASSERT_EQUAL_INT(0, user_store_playlist_set_tracks(store, pl.id, ids, 3));
    TEST_ASSERT_EQUAL_UINT(3, user_store_playlist_track_count(store, pl.id));

    n = user_store_playlist_get_tracks(store, pl.id, 0, 10, out, 8);
    TEST_ASSERT_EQUAL_UINT(3, n);
    TEST_ASSERT_EQUAL_UINT(10, out[0]);
    TEST_ASSERT_EQUAL_UINT(20, out[1]);
    TEST_ASSERT_EQUAL_UINT(30, out[2]);

    TEST_ASSERT_EQUAL_INT(0, user_store_playlist_rename(store, pl.id, "Renamed"));
    TEST_ASSERT_EQUAL_INT(0, user_store_playlist_get(store, pl.id, &pl));
    TEST_ASSERT_EQUAL_STRING("Renamed", pl.name);
    TEST_ASSERT_EQUAL_UINT(3, pl.track_count);

    TEST_ASSERT_EQUAL_INT(0, user_store_playlist_delete(store, pl.id));
    TEST_ASSERT_EQUAL_INT(-1, user_store_playlist_get(store, pl.id, &pl));
}

void test_favourites_and_history(void)
{
    uint32_t favs[4];
    user_history_entry_t hist[4];
    size_t n;

    TEST_ASSERT_EQUAL_INT(0, user_store_favourite_add(store, 7));
    TEST_ASSERT_TRUE(user_store_favourite_has(store, 7));
    TEST_ASSERT_EQUAL_INT(0, user_store_favourite_add(store, 8));
    TEST_ASSERT_EQUAL_UINT(2, user_store_favourite_count(store));
    n = user_store_favourite_list(store, 0, 10, favs, 4);
    TEST_ASSERT_EQUAL_UINT(2, n);

    TEST_ASSERT_EQUAL_INT(0, user_store_favourite_remove(store, 7));
    TEST_ASSERT_FALSE(user_store_favourite_has(store, 7));

    TEST_ASSERT_EQUAL_INT(0, user_store_history_append(store, 100));
    TEST_ASSERT_EQUAL_INT(0, user_store_history_append(store, 200));
    TEST_ASSERT_EQUAL_UINT(2, user_store_history_count(store));
    n = user_store_history_list(store, 0, 10, hist, 4);
    TEST_ASSERT_EQUAL_UINT(2, n);
    TEST_ASSERT_EQUAL_UINT(200, hist[0].track_id);
    TEST_ASSERT_EQUAL_UINT(100, hist[1].track_id);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_playlist_create_and_tracks);
    RUN_TEST(test_favourites_and_history);
    return UNITY_END();
}
