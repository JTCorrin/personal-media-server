#include "unity.h"

#include "media_server/library/catalog.h"
#include "media_server/library/catalog_store.h"
#include "media_server/media/kind.h"

#include <sqlite3.h>
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

void test_catalog_store_failure_does_not_modify_destination(void)
{
    catalog_t *loaded = catalog_create();
    sqlite3 *db = NULL;

    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "A/B/valid.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_IMAGE, "A/B/cover.jpg"));
    TEST_ASSERT_EQUAL_INT(0, catalog_store_save(db_path, "/music/a", catalog));

    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_open(db_path, &db));
    TEST_ASSERT_EQUAL_INT(
        SQLITE_OK,
        sqlite3_exec(db, "UPDATE items SET kind = 0 WHERE id = 2;", NULL, NULL,
                     NULL));
    sqlite3_close(db);

    /* A pre-existing destination must be unchanged after a partial DB read. */
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(loaded, MEDIA_KIND_AUDIO, "Existing/Album/keep.mp3"));
    TEST_ASSERT_EQUAL_INT(-1,
                          catalog_store_load(db_path, "/music/a", loaded));
    TEST_ASSERT_EQUAL_UINT(1, catalog_count(loaded));
    TEST_ASSERT_EQUAL_STRING("Existing/Album/keep.mp3",
                             catalog_get(loaded, 0)->path);

    catalog_destroy(loaded);
}

void test_catalog_store_rejects_inconsistent_next_id(void)
{
    catalog_t *loaded = catalog_create();
    sqlite3 *db = NULL;

    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "A/B/valid.mp3"));
    catalog_set_next_id(catalog, 50);
    TEST_ASSERT_EQUAL_INT(0, catalog_store_save(db_path, "/music/a", catalog));

    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_open(db_path, &db));
    TEST_ASSERT_EQUAL_INT(
        SQLITE_OK,
        sqlite3_exec(db, "UPDATE meta SET next_id = 1;", NULL, NULL, NULL));
    sqlite3_close(db);

    TEST_ASSERT_EQUAL_INT(-1,
                          catalog_store_load(db_path, "/music/a", loaded));
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

void test_catalog_store_override_round_trip_and_snapshot_preserves_it(void)
{
    catalog_t *loaded = catalog_create();
    metadata_patch_t patch = {0};
    const char *paths[] = {"Artist/Album/t1.mp3"};
    const catalog_item_t *item;

    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Artist/Album/t1.mp3"));
    TEST_ASSERT_EQUAL_INT(0, catalog_store_save(db_path, "/music/lib", catalog));

    patch.set_mask = METADATA_FIELD_TITLE | METADATA_FIELD_RELEASE_DATE;
    strcpy(patch.values.title, "Fixed");
    strcpy(patch.values.release_date, "2020");
    TEST_ASSERT_EQUAL_INT(
        0, catalog_store_patch_overrides(db_path, paths, 1, &patch));
    TEST_ASSERT_EQUAL_INT(0, catalog_store_load(db_path, "/music/lib", loaded));
    item = catalog_find_id(loaded, 1);
    TEST_ASSERT_EQUAL_STRING("Fixed", item->title);
    TEST_ASSERT_EQUAL_STRING("t1", item->scanned.title);

    TEST_ASSERT_EQUAL_INT(0, catalog_store_save(db_path, "/music/lib", loaded));
    catalog_destroy(loaded);
    loaded = catalog_create();
    TEST_ASSERT_EQUAL_INT(0, catalog_store_load(db_path, "/music/lib", loaded));
    item = catalog_find_id(loaded, 1);
    TEST_ASSERT_EQUAL_STRING("Fixed", item->title);

    memset(&patch, 0, sizeof(patch));
    patch.clear_mask = METADATA_FIELD_TITLE;
    TEST_ASSERT_EQUAL_INT(
        0, catalog_store_patch_overrides(db_path, paths, 1, &patch));
    catalog_destroy(loaded);
    loaded = catalog_create();
    TEST_ASSERT_EQUAL_INT(0, catalog_store_load(db_path, "/music/lib", loaded));
    TEST_ASSERT_EQUAL_STRING("t1", catalog_find_id(loaded, 1)->title);
    TEST_ASSERT_EQUAL_STRING("2020", catalog_find_id(loaded, 1)->release_date);
    catalog_destroy(loaded);
}

void test_catalog_store_migrates_v1(void)
{
    sqlite3 *db = NULL;
    catalog_t *loaded = catalog_create();
    const char *sql =
        "CREATE TABLE meta (id INTEGER PRIMARY KEY, library_root TEXT NOT NULL,"
        "next_id INTEGER NOT NULL,schema_version INTEGER NOT NULL);"
        "CREATE TABLE items (id INTEGER PRIMARY KEY,kind INTEGER NOT NULL,"
        "path TEXT NOT NULL UNIQUE,filename TEXT NOT NULL,artist TEXT NOT NULL,"
        "album TEXT NOT NULL,title TEXT NOT NULL);"
        "INSERT INTO meta VALUES (1,'/music/v1',2,1);"
        "INSERT INTO items VALUES (1,1,'A/B/t.mp3','t.mp3','A','B','t');";

    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_open(db_path, &db));
    TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_exec(db, sql, NULL, NULL, NULL));
    sqlite3_close(db);
    TEST_ASSERT_EQUAL_INT(0, catalog_store_load(db_path, "/music/v1", loaded));
    TEST_ASSERT_EQUAL_STRING("t", catalog_find_id(loaded, 1)->title);
    TEST_ASSERT_EQUAL_STRING("", catalog_find_id(loaded, 1)->release_date);
    catalog_destroy(loaded);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_catalog_store_round_trip);
    RUN_TEST(test_catalog_store_rejects_root_mismatch);
    RUN_TEST(test_catalog_store_failure_does_not_modify_destination);
    RUN_TEST(test_catalog_store_rejects_inconsistent_next_id);
    RUN_TEST(test_catalog_adopt_ids_reuses_paths);
    RUN_TEST(test_catalog_store_override_round_trip_and_snapshot_preserves_it);
    RUN_TEST(test_catalog_store_migrates_v1);
    return UNITY_END();
}
