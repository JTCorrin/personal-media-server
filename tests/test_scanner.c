#include "unity.h"

#include "media_server/library/catalog.h"
#include "media_server/library/scanner.h"
#include "media_server/media/kind.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
        catalog_has_path(MEDIA_KIND_AUDIO, "Artist/Album/track02.mp3"));
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

/*
 * Build a throwaway library in /tmp containing:
 *   real.mp3                      regular file (should be catalogued)
 *   outside.mp3 -> /etc/hostname  symlinked file (should be skipped)
 *   loop -> .                     symlinked directory cycle (must not hang)
 * Returns the root path in `root` (caller removes it), or false on failure.
 */
static bool make_symlink_library(char *root, size_t root_size)
{
    char path[512];
    FILE *f;

    snprintf(root, root_size, "/tmp/media_server_scanner_test_XXXXXX");
    if (mkdtemp(root) == NULL) {
        return false;
    }

    snprintf(path, sizeof(path), "%s/real.mp3", root);
    f = fopen(path, "w");
    if (f == NULL) {
        return false;
    }
    fputs("fake mp3 data", f);
    fclose(f);

    snprintf(path, sizeof(path), "%s/outside.mp3", root);
    if (symlink("/etc/hostname", path) != 0) {
        return false;
    }

    snprintf(path, sizeof(path), "%s/loop", root);
    if (symlink(".", path) != 0) {
        return false;
    }

    return true;
}

static void remove_symlink_library(const char *root)
{
    char path[512];

    snprintf(path, sizeof(path), "%s/real.mp3", root);
    unlink(path);
    snprintf(path, sizeof(path), "%s/outside.mp3", root);
    unlink(path);
    snprintf(path, sizeof(path), "%s/loop", root);
    unlink(path);
    rmdir(root);
}

void test_scanner_continues_past_unreadable_subdir(void)
{
    char root[128];
    char subdir[256];
    char track[512];
    FILE *f;

    snprintf(root, sizeof(root), "/tmp/media_server_scanner_perm_XXXXXX");
    TEST_ASSERT_NOT_NULL(mkdtemp(root));

    snprintf(track, sizeof(track), "%s/ok.mp3", root);
    f = fopen(track, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs("fake mp3 data", f);
    fclose(f);

    snprintf(subdir, sizeof(subdir), "%s/locked", root);
    TEST_ASSERT_EQUAL_INT(0, mkdir(subdir, 0700));
    TEST_ASSERT_EQUAL_INT(0, chmod(subdir, 0000));

    /* The unreadable subdirectory is skipped, not a fatal error. */
    TEST_ASSERT_EQUAL_INT(0, scanner_scan(root, catalog));
    TEST_ASSERT_TRUE(catalog_has_path(MEDIA_KIND_AUDIO, "ok.mp3"));

    chmod(subdir, 0700);
    rmdir(subdir);
    unlink(track);
    rmdir(root);
}

void test_scanner_skips_symlinks_and_survives_cycles(void)
{
    char root[128];

    TEST_ASSERT_TRUE(make_symlink_library(root, sizeof(root)));

    /* Must terminate (no infinite recursion through `loop`) and succeed. */
    TEST_ASSERT_EQUAL_INT(0, scanner_scan(root, catalog));

    /* Only the regular file gets catalogued; symlinks are skipped. */
    TEST_ASSERT_EQUAL_UINT(1, catalog_count(catalog));
    TEST_ASSERT_TRUE(catalog_has_path(MEDIA_KIND_AUDIO, "real.mp3"));
    TEST_ASSERT_FALSE(catalog_has_path(MEDIA_KIND_AUDIO, "outside.mp3"));

    remove_symlink_library(root);
}

void test_scanner_cancelable_stops_early(void)
{
    atomic_bool cancel = true;

    TEST_ASSERT_EQUAL_INT(1, scanner_scan_cancelable(FIXTURE_LIBRARY, catalog, &cancel));
    TEST_ASSERT_EQUAL_UINT(0, catalog_count(catalog));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_scanner_finds_audio_and_images);
    RUN_TEST(test_scanner_ignores_non_media);
    RUN_TEST(test_scanner_rejects_bad_args);
    RUN_TEST(test_scanner_continues_past_unreadable_subdir);
    RUN_TEST(test_scanner_skips_symlinks_and_survives_cycles);
    RUN_TEST(test_scanner_cancelable_stops_early);
    return UNITY_END();
}
