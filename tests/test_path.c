#include "unity.h"

#include "media_server/util/path.h"

void setUp(void) {}
void tearDown(void) {}

void test_path_join_inserts_slash(void)
{
    char out[64];

    TEST_ASSERT_EQUAL_INT(0, path_join(out, sizeof(out), "/music", "a.mp3"));
    TEST_ASSERT_EQUAL_STRING("/music/a.mp3", out);
}

void test_path_join_avoids_double_slash(void)
{
    char out[64];

    TEST_ASSERT_EQUAL_INT(0, path_join(out, sizeof(out), "/music/", "a.mp3"));
    TEST_ASSERT_EQUAL_STRING("/music/a.mp3", out);
}

void test_path_join_absolute_name_wins(void)
{
    char out[64];

    TEST_ASSERT_EQUAL_INT(0, path_join(out, sizeof(out), "/music", "/other/a.mp3"));
    TEST_ASSERT_EQUAL_STRING("/other/a.mp3", out);
}

void test_path_join_rejects_overflow(void)
{
    char out[8];

    TEST_ASSERT_EQUAL_INT(-1, path_join(out, sizeof(out), "/music", "verylong.mp3"));
}

void test_path_basename(void)
{
    TEST_ASSERT_EQUAL_STRING("song.mp3", path_basename("/music/album/song.mp3"));
    TEST_ASSERT_EQUAL_STRING("song.mp3", path_basename("song.mp3"));
}

void test_path_dirname(void)
{
    char out[64];

    TEST_ASSERT_EQUAL_INT(0, path_dirname(out, sizeof(out), "Artist/Album/track.mp3"));
    TEST_ASSERT_EQUAL_STRING("Artist/Album", out);
    TEST_ASSERT_EQUAL_INT(0, path_dirname(out, sizeof(out), "track.mp3"));
    TEST_ASSERT_EQUAL_STRING("", out);
    TEST_ASSERT_EQUAL_INT(-1, path_dirname(out, 4, "Artist/Album/track.mp3"));
}

void test_path_common_dir(void)
{
    char out[64];

    TEST_ASSERT_EQUAL_INT(
        0, path_common_dir(out, sizeof(out), "Artist/Album/CD1",
                           "Artist/Album/CD2"));
    TEST_ASSERT_EQUAL_STRING("Artist/Album", out);

    TEST_ASSERT_EQUAL_INT(
        0, path_common_dir(out, sizeof(out), "Artist/Album", "Artist/Album"));
    TEST_ASSERT_EQUAL_STRING("Artist/Album", out);

    TEST_ASSERT_EQUAL_INT(
        0, path_common_dir(out, sizeof(out), "Artist/Album",
                           "Artist/Album/CD1"));
    TEST_ASSERT_EQUAL_STRING("Artist/Album", out);

    TEST_ASSERT_EQUAL_INT(
        0, path_common_dir(out, sizeof(out), "Artist/Album",
                           "Artist/AlbumExtra"));
    TEST_ASSERT_EQUAL_STRING("Artist", out);

    TEST_ASSERT_EQUAL_INT(0, path_common_dir(out, sizeof(out), "A/X", "B/Y"));
    TEST_ASSERT_EQUAL_STRING("", out);

    TEST_ASSERT_EQUAL_INT(-1, path_common_dir(out, 4, "Artist/Album/CD1",
                                              "Artist/Album/CD2"));
    TEST_ASSERT_EQUAL_INT(-1, path_common_dir(NULL, 8, "a", "b"));
    TEST_ASSERT_EQUAL_INT(-1, path_common_dir(out, sizeof(out), NULL, "b"));
}

void test_path_extension(void)
{
    TEST_ASSERT_EQUAL_STRING(".mp3", path_extension("/music/song.mp3"));
    TEST_ASSERT_EQUAL_STRING(".FLAC", path_extension("track.FLAC"));
    TEST_ASSERT_NULL(path_extension("/music/album"));
    TEST_ASSERT_NULL(path_extension("/music/.hidden"));
}

void test_path_has_extension(void)
{
    TEST_ASSERT_TRUE(path_has_extension("/a/song.mp3", "mp3"));
    TEST_ASSERT_TRUE(path_has_extension("/a/song.MP3", ".mp3"));
    TEST_ASSERT_FALSE(path_has_extension("/a/song.mp3", "flac"));
    TEST_ASSERT_FALSE(path_has_extension("/a/song", "mp3"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_path_join_inserts_slash);
    RUN_TEST(test_path_join_avoids_double_slash);
    RUN_TEST(test_path_join_absolute_name_wins);
    RUN_TEST(test_path_join_rejects_overflow);
    RUN_TEST(test_path_basename);
    RUN_TEST(test_path_dirname);
    RUN_TEST(test_path_common_dir);
    RUN_TEST(test_path_extension);
    RUN_TEST(test_path_has_extension);
    return UNITY_END();
}
