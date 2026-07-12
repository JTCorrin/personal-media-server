#include "unity.h"

#include "media_server/library/catalog.h"
#include "media_server/library/scanner.h"
#include "media_server/media/kind.h"

#include <stdbool.h>
#include <string.h>

static catalog_t *catalog;

/* Relative to project root when tests are run via `make test`. */
static const char *FIXTURE_LIBRARY = "tests/fixtures/library";

void setUp(void)
{
    catalog = catalog_create();
    TEST_ASSERT_NOT_NULL(catalog);
}

void tearDown(void)
{
    catalog_destroy(catalog);
}

static bool catalog_has_path(media_kind_t kind, const char *rel_path)
{
    for (size_t i = 0; i < catalog_count(catalog); i++) {
        const catalog_item_t *item = catalog_get(catalog, i);
        if (item != NULL && item->kind == kind &&
            strcmp(item->path, rel_path) == 0) {
            return true;
        }
    }
    return false;
}

void test_scanner_finds_audio_and_images(void)
{
    TEST_ASSERT_EQUAL_INT(0, scanner_scan(FIXTURE_LIBRARY, catalog));

    TEST_ASSERT_EQUAL_UINT(3, catalog_count(catalog));
    TEST_ASSERT_EQUAL_UINT(2, catalog_count_kind(catalog, MEDIA_KIND_AUDIO));
    TEST_ASSERT_EQUAL_UINT(1, catalog_count_kind(catalog, MEDIA_KIND_IMAGE));

    TEST_ASSERT_TRUE(
        catalog_has_path(MEDIA_KIND_AUDIO, "Artist/Album/track01.mp3"));
    TEST_ASSERT_TRUE(
        catalog_has_path(MEDIA_KIND_AUDIO, "Artist/Album/track02.flac"));
    TEST_ASSERT_TRUE(
        catalog_has_path(MEDIA_KIND_IMAGE, "Artist/Album/cover.jpg"));
}

void test_scanner_ignores_non_media(void)
{
    TEST_ASSERT_EQUAL_INT(0, scanner_scan(FIXTURE_LIBRARY, catalog));

    TEST_ASSERT_FALSE(
        catalog_has_path(MEDIA_KIND_AUDIO, "Artist/Album/notes.txt"));
    TEST_ASSERT_FALSE(catalog_has_path(MEDIA_KIND_IMAGE, "skip.me"));
    for (size_t i = 0; i < catalog_count(catalog); i++) {
        const catalog_item_t *item = catalog_get(catalog, i);
        TEST_ASSERT_NOT_NULL(item);
        TEST_ASSERT_TRUE(strstr(item->path, "notes.txt") == NULL);
        TEST_ASSERT_TRUE(strstr(item->path, "skip.me") == NULL);
    }
}

void test_scanner_rejects_bad_args(void)
{
    TEST_ASSERT_EQUAL_INT(-1, scanner_scan(NULL, catalog));
    TEST_ASSERT_EQUAL_INT(-1, scanner_scan(FIXTURE_LIBRARY, NULL));
    TEST_ASSERT_EQUAL_INT(-1, scanner_scan("/no/such/library/root", catalog));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_scanner_finds_audio_and_images);
    RUN_TEST(test_scanner_ignores_non_media);
    RUN_TEST(test_scanner_rejects_bad_args);
    return UNITY_END();
}
