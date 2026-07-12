#include "unity.h"

#include "media_server/library/catalog.h"
#include "media_server/library/search.h"
#include "media_server/media/kind.h"

static catalog_t *catalog;

void setUp(void)
{
    catalog = catalog_create();
    TEST_ASSERT_NOT_NULL(catalog);
}

void tearDown(void)
{
    catalog_destroy(catalog);
}

void test_search_track_matches_fields(void)
{
    const catalog_item_t *item;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Artist/Album/HelixSong.mp3"));
    item = catalog_get(catalog, 0);
    TEST_ASSERT_NOT_NULL(item);

    TEST_ASSERT_TRUE(search_track_matches(item, "helix"));
    TEST_ASSERT_TRUE(search_track_matches(item, "ARTIST"));
    TEST_ASSERT_TRUE(search_track_matches(item, "album"));
    TEST_ASSERT_FALSE(search_track_matches(item, "xyz"));
    TEST_ASSERT_FALSE(search_track_matches(item, ""));
}

void test_search_ignores_images(void)
{
    const catalog_item_t *item;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_IMAGE, "Artist/Album/cover.jpg"));
    item = catalog_get(catalog, 0);
    TEST_ASSERT_FALSE(search_track_matches(item, "Artist"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_search_track_matches_fields);
    RUN_TEST(test_search_ignores_images);
    return UNITY_END();
}
