#include "unity.h"

#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"
#include "media_server/library/catalog_store.h"
#include "media_server/library/runtime.h"
#include "media_server/library/scanner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static app_context_t ctx;
static catalog_t *catalog;
static browse_index_t *browse;

static void sleep_ms(int ms)
{
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static int write_hold(const char *library_dir)
{
    char path[512];
    FILE *fp;

    if (snprintf(path, sizeof(path), "%s/.media_server_scan_hold", library_dir) >=
        (int)sizeof(path)) {
        return -1;
    }
    fp = fopen(path, "w");
    if (fp == NULL) {
        return -1;
    }
    fclose(fp);
    return 0;
}

static void clear_hold(const char *library_dir)
{
    char path[512];

    if (snprintf(path, sizeof(path), "%s/.media_server_scan_hold", library_dir) <
        (int)sizeof(path)) {
        remove(path);
    }
}

void setUp(void)
{
    memset(&ctx, 0, sizeof(ctx));
    catalog = catalog_create();
    TEST_ASSERT_NOT_NULL(catalog);
    TEST_ASSERT_EQUAL_INT(0, scanner_scan("tests/fixtures/library", catalog));
    browse = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(browse);

    ctx.catalog = catalog;
    ctx.browse = browse;
    ctx.library_dir = "tests/fixtures/library";
    TEST_ASSERT_EQUAL_INT(0, library_runtime_init(&ctx));
    catalog = NULL;
    browse = NULL;
}

void tearDown(void)
{
    clear_hold(ctx.library_dir);
    library_runtime_shutdown(&ctx);
    browse_index_destroy(ctx.browse);
    catalog_destroy(ctx.catalog);
    browse_index_destroy(browse);
    catalog_destroy(catalog);
}

void test_scan_request_rejects_missing_library(void)
{
    app_context_t empty = {0};

    TEST_ASSERT_EQUAL_INT(0, library_runtime_init(&empty));
    TEST_ASSERT_EQUAL_INT(-1, library_scan_request(&empty, false));
    library_runtime_shutdown(&empty);
}

void test_scan_request_starts_and_completes(void)
{
    library_status_t st;
    int rc;

    rc = library_scan_request(&ctx, false);
    TEST_ASSERT_EQUAL_INT(0, rc);

    for (int i = 0; i < 200; i++) {
        library_status_get(&ctx, &st);
        if (!st.scanning) {
            break;
        }
        sleep_ms(10);
    }

    library_status_get(&ctx, &st);
    TEST_ASSERT_FALSE(st.scanning);
    TEST_ASSERT_TRUE(st.last_scan_ok);
    TEST_ASSERT_TRUE(st.track_count >= 2);
}

void test_scan_request_busy_and_force(void)
{
    library_status_t st;
    int first;
    int second;
    int forced;
    bool saw_scanning = false;

    TEST_ASSERT_EQUAL_INT(0, write_hold(ctx.library_dir));

    first = library_scan_request(&ctx, false);
    TEST_ASSERT_EQUAL_INT(0, first);

    for (int i = 0; i < 200; i++) {
        library_status_get(&ctx, &st);
        if (st.scanning) {
            saw_scanning = true;
            break;
        }
        sleep_ms(5);
    }
    TEST_ASSERT_TRUE(saw_scanning);

    second = library_scan_request(&ctx, false);
    TEST_ASSERT_EQUAL_INT(1, second);

    forced = library_scan_request(&ctx, true);
    TEST_ASSERT_EQUAL_INT(2, forced);

    clear_hold(ctx.library_dir);

    for (int i = 0; i < 200; i++) {
        library_status_get(&ctx, &st);
        if (!st.scanning) {
            break;
        }
        sleep_ms(10);
    }
    library_status_get(&ctx, &st);
    TEST_ASSERT_FALSE(st.scanning);
    TEST_ASSERT_TRUE(st.last_scan_ok);
}

void test_completed_workers_are_reaped_before_next_scan(void)
{
    library_status_t st;

    for (int run = 0; run < 3; run++) {
        TEST_ASSERT_EQUAL_INT(0, library_scan_request(&ctx, false));
        for (int i = 0; i < 200; i++) {
            library_status_get(&ctx, &st);
            if (!st.scanning) {
                break;
            }
            sleep_ms(10);
        }
        library_status_get(&ctx, &st);
        TEST_ASSERT_FALSE(st.scanning);
        TEST_ASSERT_TRUE(st.last_scan_ok);
    }
}

void test_metadata_patch_is_ephemeral_without_catalog_db(void)
{
    const catalog_item_t *original =
        catalog_find_path(ctx.catalog, "Artist/Album/track01.mp3");
    metadata_patch_t patch = {0};
    catalog_item_t updated;
    library_status_t st;
    uint32_t id;

    TEST_ASSERT_NOT_NULL(original);
    id = original->id;
    patch.set_mask = METADATA_FIELD_TITLE | METADATA_FIELD_GENRE;
    strcpy(patch.values.title, "Fixed title");
    strcpy(patch.values.genre, "Test genre");
    TEST_ASSERT_EQUAL_INT(
        0, library_metadata_patch_track(&ctx, id, &patch, &updated));
    TEST_ASSERT_EQUAL_STRING("Fixed title", updated.title);
    TEST_ASSERT_BITS(METADATA_FIELD_TITLE, METADATA_FIELD_TITLE,
                     updated.override_mask);

    TEST_ASSERT_EQUAL_INT(0, library_scan_request(&ctx, false));
    for (int i = 0; i < 200; i++) {
        library_status_get(&ctx, &st);
        if (!st.scanning) {
            break;
        }
        sleep_ms(10);
    }
    api_context_lock(&ctx);
    original = catalog_find_id(ctx.catalog, id);
    TEST_ASSERT_NOT_NULL(original);
    TEST_ASSERT_EQUAL_STRING("track01", original->title);
    api_context_unlock(&ctx);
}

void test_metadata_patch_persists_with_catalog_db(void)
{
    char path[96];
    metadata_patch_t patch = {0};
    catalog_t *loaded = catalog_create();
    const catalog_item_t *item =
        catalog_find_path(ctx.catalog, "Artist/Album/track01.mp3");

    snprintf(path, sizeof(path), "/tmp/ms_runtime_metadata_%d.db", (int)getpid());
    unlink(path);
    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_NOT_NULL(item);
    TEST_ASSERT_EQUAL_INT(
        0, catalog_store_save(path, ctx.library_dir, ctx.catalog));
    ctx.catalog_db_path = path;
    patch.set_mask = METADATA_FIELD_TITLE;
    strcpy(patch.values.title, "Persistent title");
    TEST_ASSERT_EQUAL_INT(
        0, library_metadata_patch_track(&ctx, item->id, &patch, NULL));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_store_load(path, ctx.library_dir, loaded));
    TEST_ASSERT_EQUAL_STRING(
        "Persistent title",
        catalog_find_path(loaded, "Artist/Album/track01.mp3")->title);
    catalog_destroy(loaded);
    ctx.catalog_db_path = NULL;
    unlink(path);
}

static int copy_file(const char *src, const char *dst)
{
    FILE *in;
    FILE *out;
    char buf[4096];
    size_t n;

    in = fopen(src, "rb");
    if (in == NULL) {
        return -1;
    }
    out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

void test_album_cover_put_writes_and_rescans(void)
{
    char root[] = "/tmp/media_server_cover_XXXXXX";
    char album_dir[512];
    char track_src[512];
    char track_dst[512];
    char rel_path[256];
    char cover_path[512];
    char marker[] = "COVERTEST";
    app_context_t local = {0};
    catalog_t *local_catalog;
    browse_index_t *local_browse;
    library_status_t st;
    const browse_album_t *album;
    FILE *fp;
    char read_buf[32];
    int rc;

    TEST_ASSERT_NOT_NULL(mkdtemp(root));
    TEST_ASSERT_TRUE(snprintf(album_dir, sizeof(album_dir), "%s/Artist/Album", root) <
                     (int)sizeof(album_dir));
    {
        char artist_dir[512];
        TEST_ASSERT_TRUE(snprintf(artist_dir, sizeof(artist_dir), "%s/Artist", root) <
                         (int)sizeof(artist_dir));
        TEST_ASSERT_EQUAL_INT(0, mkdir(artist_dir, 0755));
        TEST_ASSERT_EQUAL_INT(0, mkdir(album_dir, 0755));
    }
    TEST_ASSERT_TRUE(
        snprintf(track_src, sizeof(track_src),
                 "tests/fixtures/library/Artist/Album/track01.mp3") <
        (int)sizeof(track_src));
    TEST_ASSERT_TRUE(snprintf(track_dst, sizeof(track_dst), "%s/track01.mp3",
                              album_dir) < (int)sizeof(track_dst));
    TEST_ASSERT_EQUAL_INT(0, copy_file(track_src, track_dst));

    local_catalog = catalog_create();
    TEST_ASSERT_NOT_NULL(local_catalog);
    TEST_ASSERT_EQUAL_INT(0, scanner_scan(root, local_catalog));
    local_browse = browse_index_build(local_catalog);
    TEST_ASSERT_NOT_NULL(local_browse);
    local.catalog = local_catalog;
    local.browse = local_browse;
    local.library_dir = root;
    TEST_ASSERT_EQUAL_INT(0, library_runtime_init(&local));

    album = browse_album_get(local.browse, 0);
    TEST_ASSERT_NOT_NULL(album);
    TEST_ASSERT_EQUAL_UINT(0, album->cover_id);

    rc = library_album_cover_put(&local, album->id, marker, sizeof(marker) - 1, "jpg",
                                 rel_path, sizeof(rel_path));
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_STRING("Artist/Album/cover.jpg", rel_path);

    TEST_ASSERT_TRUE(snprintf(cover_path, sizeof(cover_path), "%s/cover.jpg",
                              album_dir) < (int)sizeof(cover_path));
    fp = fopen(cover_path, "rb");
    TEST_ASSERT_NOT_NULL(fp);
    TEST_ASSERT_EQUAL_UINT(sizeof(marker) - 1, fread(read_buf, 1, sizeof(read_buf), fp));
    fclose(fp);
    TEST_ASSERT_EQUAL_INT(0, memcmp(read_buf, marker, sizeof(marker) - 1));

    for (int i = 0; i < 200; i++) {
        library_status_get(&local, &st);
        if (!st.scanning) {
            break;
        }
        sleep_ms(10);
    }
    library_status_get(&local, &st);
    TEST_ASSERT_FALSE(st.scanning);
    TEST_ASSERT_TRUE(st.last_scan_ok);
    album = browse_album_get(local.browse, 0);
    TEST_ASSERT_NOT_NULL(album);
    TEST_ASSERT_TRUE(album->cover_id != 0);

    library_runtime_shutdown(&local);
    browse_index_destroy(local.browse);
    catalog_destroy(local.catalog);
    remove(cover_path);
    remove(track_dst);
    rmdir(album_dir);
    {
        char artist_dir[512];
        snprintf(artist_dir, sizeof(artist_dir), "%s/Artist", root);
        rmdir(artist_dir);
    }
    rmdir(root);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_scan_request_rejects_missing_library);
    RUN_TEST(test_scan_request_starts_and_completes);
    RUN_TEST(test_scan_request_busy_and_force);
    RUN_TEST(test_completed_workers_are_reaped_before_next_scan);
    RUN_TEST(test_metadata_patch_is_ephemeral_without_catalog_db);
    RUN_TEST(test_metadata_patch_persists_with_catalog_db);
    RUN_TEST(test_album_cover_put_writes_and_rescans);
    return UNITY_END();
}
