#include "unity.h"

#include "media_server/library/catalog.h"

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

void test_catalog_starts_empty(void)
{
    TEST_ASSERT_EQUAL_UINT(0, catalog_count(catalog));
    TEST_ASSERT_NULL(catalog_get(catalog, 0));
}

void test_catalog_add_assigns_monotonic_ids(void)
{
    const catalog_item_t *a;
    const catalog_item_t *b;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Artist/Album/track01.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_IMAGE, "Artist/Album/cover.jpg"));

    TEST_ASSERT_EQUAL_UINT(2, catalog_count(catalog));

    a = catalog_get(catalog, 0);
    b = catalog_get(catalog, 1);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_UINT(1, a->id);
    TEST_ASSERT_EQUAL_UINT(2, b->id);
    TEST_ASSERT_EQUAL(MEDIA_KIND_AUDIO, a->kind);
    TEST_ASSERT_EQUAL_STRING("Artist/Album/track01.mp3", a->path);
    TEST_ASSERT_EQUAL_STRING("track01.mp3", a->filename);
    TEST_ASSERT_EQUAL_STRING("Artist", a->artist);
    TEST_ASSERT_EQUAL_STRING("Album", a->album);
    TEST_ASSERT_EQUAL_STRING("track01", a->title);
    TEST_ASSERT_EQUAL_STRING("cover.jpg", b->filename);
    TEST_ASSERT_EQUAL_STRING("Artist", b->artist);
    TEST_ASSERT_EQUAL_STRING("Album", b->album);
    TEST_ASSERT_EQUAL_STRING("cover", b->title);
}

void test_catalog_find_id(void)
{
    const catalog_item_t *found;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "a.mp3"));
    found = catalog_find_id(catalog, 1);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("a.mp3", found->path);
    TEST_ASSERT_NULL(catalog_find_id(catalog, 99));
}

void test_catalog_rejects_invalid(void)
{
    TEST_ASSERT_EQUAL_INT(-1, catalog_add(catalog, MEDIA_KIND_NONE, "a.mp3"));
    TEST_ASSERT_EQUAL_INT(-1, catalog_add(catalog, MEDIA_KIND_AUDIO, NULL));
    TEST_ASSERT_EQUAL_INT(-1, catalog_add(catalog, MEDIA_KIND_AUDIO, ""));
}

void test_catalog_count_kind(void)
{
    TEST_ASSERT_EQUAL_INT(0, catalog_add(catalog, MEDIA_KIND_AUDIO, "a.mp3"));
    TEST_ASSERT_EQUAL_INT(0, catalog_add(catalog, MEDIA_KIND_AUDIO, "b.flac"));
    TEST_ASSERT_EQUAL_INT(0, catalog_add(catalog, MEDIA_KIND_IMAGE, "c.jpg"));

    TEST_ASSERT_EQUAL_UINT(2, catalog_count_kind(catalog, MEDIA_KIND_AUDIO));
    TEST_ASSERT_EQUAL_UINT(1, catalog_count_kind(catalog, MEDIA_KIND_IMAGE));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_catalog_starts_empty);
    RUN_TEST(test_catalog_add_assigns_monotonic_ids);
    RUN_TEST(test_catalog_find_id);
    RUN_TEST(test_catalog_rejects_invalid);
    RUN_TEST(test_catalog_count_kind);
    return UNITY_END();
}
