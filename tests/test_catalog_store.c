#include "unity.h"

#include "media_server/library/catalog.h"
#include "media_server/library/catalog_store.h"
#include "media_server/media/kind.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char db_path[64];
static catalog_t *catalog;

void setUp(void)
{
    catalog = catalog_create();
    TEST_ASSERT_NOT_NULL(catalog);
    snprintf(db_path, sizeof(db_path), "/tmp/ms_catalog_test_%d.db", (int)getpid());
    unlink(db_path);
}

void tearDown(void)
{
    catalog_destroy(catalog);
    unlink(db_path);
}

void test_catalog_store_round_trip(void)
{
    catalog_t *loaded = catalog_create();
    const catalog_item_t *a;
    const catalog_item_t *b;

    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Artist/Album/t1.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_IMAGE, "Artist/Album/cover.jpg"));

    TEST_ASSERT_EQUAL_INT(
        0, catalog_store_save(db_path, "/music/lib", catalog));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_store_load(db_path, "/music/lib", loaded));

    TEST_ASSERT_EQUAL_UINT(2, catalog_count(loaded));
    a = catalog_get(loaded, 0);
    b = catalog_get(loaded, 1);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_UINT(1, a->id);
    TEST_ASSERT_EQUAL_STRING("Artist/Album/t1.mp3", a->path);
    TEST_ASSERT_EQUAL_UINT(2, b->id);
    TEST_ASSERT_EQUAL_STRING("Artist/Album/cover.jpg", b->path);
    TEST_ASSERT_EQUAL_UINT(3, catalog_next_id(loaded));

    catalog_destroy(loaded);
}

void test_catalog_store_rejects_root_mismatch(void)
{
    catalog_t *loaded = catalog_create();

    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "A/B/t.mp3"));
    TEST_ASSERT_EQUAL_INT(0, catalog_store_save(db_path, "/music/a", catalog));
    TEST_ASSERT_EQUAL_INT(-1, catalog_store_load(db_path, "/music/b", loaded));
    TEST_ASSERT_EQUAL_UINT(0, catalog_count(loaded));

    catalog_destroy(loaded);
}

void test_catalog_adopt_ids_reuses_paths(void)
{
    catalog_t *fresh = catalog_create();
    const catalog_item_t *item;

    TEST_ASSERT_NOT_NULL(fresh);

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Artist/Album/t1.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Artist/Album/t2.mp3"));
    /* Simulate prior ids by adopting against empty then bumping via add_item path:
     * force next_id higher by adding then we'll rebuild fresh. */
    catalog_set_next_id(catalog, 10);

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(fresh, MEDIA_KIND_AUDIO, "Artist/Album/t2.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(fresh, MEDIA_KIND_AUDIO, "Artist/Album/t3.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(fresh, MEDIA_KIND_AUDIO, "Artist/Album/t1.mp3"));

    TEST_ASSERT_EQUAL_INT(0, catalog_adopt_ids(fresh, catalog));

    item = catalog_find_path(fresh, "Artist/Album/t1.mp3");
    TEST_ASSERT_NOT_NULL(item);
    TEST_ASSERT_EQUAL_UINT(1, item->id);

    item = catalog_find_path(fresh, "Artist/Album/t2.mp3");
    TEST_ASSERT_NOT_NULL(item);
    TEST_ASSERT_EQUAL_UINT(2, item->id);

    item = catalog_find_path(fresh, "Artist/Album/t3.mp3");
    TEST_ASSERT_NOT_NULL(item);
    TEST_ASSERT_EQUAL_UINT(10, item->id);

    TEST_ASSERT_EQUAL_UINT(11, catalog_next_id(fresh));
    catalog_destroy(fresh);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_catalog_store_round_trip);
    RUN_TEST(test_catalog_store_rejects_root_mismatch);
    RUN_TEST(test_catalog_adopt_ids_reuses_paths);
    return UNITY_END();
}
