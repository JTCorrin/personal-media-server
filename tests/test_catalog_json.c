#include "unity.h"

#include "media_server/api/catalog_json.h"
#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"
#include "media_server/media/kind.h"
#include "media_server/util/string_buf.h"

#include <string.h>

static catalog_t *catalog;
static browse_index_t *browse;
static string_buf_t sb;

void setUp(void)
{
    catalog = catalog_create();
    browse = NULL;
    memset(&sb, 0, sizeof(sb));
    TEST_ASSERT_NOT_NULL(catalog);
    TEST_ASSERT_EQUAL_INT(0, string_buf_init(&sb, 256));
}

void tearDown(void)
{
    string_buf_free(&sb);
    browse_index_destroy(browse);
    catalog_destroy(catalog);
}

void test_track_json_includes_album_and_cover_ids(void)
{
    const catalog_item_t *track;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Artist/Album/track.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_IMAGE, "Artist/Album/cover.jpg"));
    browse = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(browse);
    track = catalog_get(catalog, 0);

    TEST_ASSERT_EQUAL_INT(0, append_catalog_item_json(&sb, track, browse));
    TEST_ASSERT_NOT_NULL(strstr(string_buf_cstr(&sb), "\"album_id\":1"));
    TEST_ASSERT_NOT_NULL(strstr(string_buf_cstr(&sb), "\"cover_id\":2"));
}

void test_track_json_includes_null_cover_without_image(void)
{
    const catalog_item_t *track;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Artist/Album/track.mp3"));
    browse = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(browse);
    track = catalog_get(catalog, 0);

    TEST_ASSERT_EQUAL_INT(0, append_catalog_item_json(&sb, track, browse));
    TEST_ASSERT_NOT_NULL(strstr(string_buf_cstr(&sb), "\"album_id\":1"));
    TEST_ASSERT_NOT_NULL(strstr(string_buf_cstr(&sb), "\"cover_id\":null"));
}

void test_track_json_includes_null_ids_without_album(void)
{
    const catalog_item_t *track;

    TEST_ASSERT_EQUAL_INT(0, catalog_add(catalog, MEDIA_KIND_AUDIO, "loose.mp3"));
    browse = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(browse);
    track = catalog_get(catalog, 0);

    TEST_ASSERT_EQUAL_INT(0, append_catalog_item_json(&sb, track, browse));
    TEST_ASSERT_NOT_NULL(strstr(string_buf_cstr(&sb), "\"album_id\":null"));
    TEST_ASSERT_NOT_NULL(strstr(string_buf_cstr(&sb), "\"cover_id\":null"));
}

void test_image_json_does_not_include_track_relationships(void)
{
    const catalog_item_t *image;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_IMAGE, "Artist/Album/cover.jpg"));
    browse = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(browse);
    image = catalog_get(catalog, 0);

    TEST_ASSERT_EQUAL_INT(0, append_catalog_item_json(&sb, image, browse));
    TEST_ASSERT_NULL(strstr(string_buf_cstr(&sb), "\"album_id\""));
    TEST_ASSERT_NULL(strstr(string_buf_cstr(&sb), "\"cover_id\""));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_track_json_includes_album_and_cover_ids);
    RUN_TEST(test_track_json_includes_null_cover_without_image);
    RUN_TEST(test_track_json_includes_null_ids_without_album);
    RUN_TEST(test_image_json_does_not_include_track_relationships);
    return UNITY_END();
}
