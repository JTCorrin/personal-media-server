#include "unity.h"

#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"
#include "media_server/media/kind.h"

static catalog_t *catalog;
static browse_index_t *index;

void setUp(void)
{
    catalog = catalog_create();
    index = NULL;
    TEST_ASSERT_NOT_NULL(catalog);
}

void tearDown(void)
{
    browse_index_destroy(index);
    catalog_destroy(catalog);
}

void test_browse_empty_catalog(void)
{
    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);
    TEST_ASSERT_EQUAL_UINT(0, browse_artist_count(index));
    TEST_ASSERT_EQUAL_UINT(0, browse_album_count(index));
}

void test_browse_groups_artist_and_album(void)
{
    const browse_artist_t *artist;
    const browse_album_t *album;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Artist/Album/track01.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Artist/Album/track02.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_IMAGE, "Artist/Album/cover.jpg"));

    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);

    TEST_ASSERT_EQUAL_UINT(1, browse_artist_count(index));
    TEST_ASSERT_EQUAL_UINT(1, browse_album_count(index));

    artist = browse_artist_get(index, 0);
    TEST_ASSERT_NOT_NULL(artist);
    TEST_ASSERT_EQUAL_UINT(1, artist->id);
    TEST_ASSERT_EQUAL_STRING("Artist", artist->name);
    TEST_ASSERT_EQUAL_UINT(1, artist->album_count);
    TEST_ASSERT_EQUAL_UINT(2, artist->track_count);

    album = browse_album_get(index, 0);
    TEST_ASSERT_NOT_NULL(album);
    TEST_ASSERT_EQUAL_UINT(1, album->id);
    TEST_ASSERT_EQUAL_STRING("Album", album->name);
    TEST_ASSERT_EQUAL_STRING("Artist", album->artist);
    TEST_ASSERT_EQUAL_UINT(1, album->artist_id);
    TEST_ASSERT_EQUAL_UINT(2, album->track_count);
}

void test_browse_ignores_loose_files_without_album(void)
{
    TEST_ASSERT_EQUAL_INT(0, catalog_add(catalog, MEDIA_KIND_AUDIO, "loose.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Solo/song.mp3"));

    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);

    /* loose.mp3 has no artist; Solo/song.mp3 has artist but no album */
    TEST_ASSERT_EQUAL_UINT(1, browse_artist_count(index));
    TEST_ASSERT_EQUAL_STRING("Solo", browse_artist_get(index, 0)->name);
    TEST_ASSERT_EQUAL_UINT(1, browse_artist_get(index, 0)->track_count);
    TEST_ASSERT_EQUAL_UINT(0, browse_artist_get(index, 0)->album_count);
    TEST_ASSERT_EQUAL_UINT(0, browse_album_count(index));
}

void test_browse_find_and_owns_item(void)
{
    const browse_album_t *album;
    const catalog_item_t *track;
    const catalog_item_t *other;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "A/One/t1.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "B/Two/t2.mp3"));

    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);

    album = browse_album_find_id(index, 1);
    TEST_ASSERT_NOT_NULL(album);
    TEST_ASSERT_EQUAL_STRING("One", album->name);

    track = catalog_get(catalog, 0);
    other = catalog_get(catalog, 1);
    TEST_ASSERT_TRUE(browse_album_owns_item(album, track));
    TEST_ASSERT_FALSE(browse_album_owns_item(album, other));
    TEST_ASSERT_NULL(browse_artist_find_id(index, 99));
    TEST_ASSERT_NULL(browse_album_find_id(index, 99));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_browse_empty_catalog);
    RUN_TEST(test_browse_groups_artist_and_album);
    RUN_TEST(test_browse_ignores_loose_files_without_album);
    RUN_TEST(test_browse_find_and_owns_item);
    return UNITY_END();
}
