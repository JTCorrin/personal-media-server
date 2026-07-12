#include "unity.h"

#include "media_server/media/path_meta.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_path_meta_artist_album_track(void)
{
    char artist[64];
    char album[64];
    char title[64];

    TEST_ASSERT_EQUAL_INT(
        0, media_path_meta("Artist/Album/track01.mp3", artist, sizeof(artist),
                           album, sizeof(album), title, sizeof(title)));
    TEST_ASSERT_EQUAL_STRING("Artist", artist);
    TEST_ASSERT_EQUAL_STRING("Album", album);
    TEST_ASSERT_EQUAL_STRING("track01", title);
}

void test_path_meta_artist_only(void)
{
    char artist[64];
    char album[64];
    char title[64];

    TEST_ASSERT_EQUAL_INT(
        0, media_path_meta("Artist/song.mp3", artist, sizeof(artist), album,
                           sizeof(album), title, sizeof(title)));
    TEST_ASSERT_EQUAL_STRING("Artist", artist);
    TEST_ASSERT_EQUAL_STRING("", album);
    TEST_ASSERT_EQUAL_STRING("song", title);
}

void test_path_meta_loose_file(void)
{
    char artist[64];
    char album[64];
    char title[64];

    TEST_ASSERT_EQUAL_INT(0, media_path_meta("loose.mp3", artist, sizeof(artist),
                                             album, sizeof(album), title,
                                             sizeof(title)));
    TEST_ASSERT_EQUAL_STRING("", artist);
    TEST_ASSERT_EQUAL_STRING("", album);
    TEST_ASSERT_EQUAL_STRING("loose", title);
}

void test_path_meta_deeper_folders(void)
{
    char artist[64];
    char album[64];
    char title[64];

    TEST_ASSERT_EQUAL_INT(
        0, media_path_meta("A/B/C/x.mp3", artist, sizeof(artist), album,
                           sizeof(album), title, sizeof(title)));
    TEST_ASSERT_EQUAL_STRING("A", artist);
    TEST_ASSERT_EQUAL_STRING("B", album);
    TEST_ASSERT_EQUAL_STRING("x", title);
}

void test_path_meta_truncates(void)
{
    char artist[4];
    char album[4];
    char title[4];

    TEST_ASSERT_EQUAL_INT(
        0, media_path_meta("LongArtist/LongAlbum/longtitle.mp3", artist,
                           sizeof(artist), album, sizeof(album), title,
                           sizeof(title)));
    TEST_ASSERT_EQUAL_STRING("Lon", artist);
    TEST_ASSERT_EQUAL_STRING("Lon", album);
    TEST_ASSERT_EQUAL_STRING("lon", title);
}

void test_path_meta_rejects_bad_args(void)
{
    char artist[8];
    char album[8];
    char title[8];

    TEST_ASSERT_EQUAL_INT(-1, media_path_meta(NULL, artist, sizeof(artist), album,
                                              sizeof(album), title, sizeof(title)));
    TEST_ASSERT_EQUAL_INT(-1, media_path_meta("", artist, sizeof(artist), album,
                                              sizeof(album), title, sizeof(title)));
    TEST_ASSERT_EQUAL_INT(-1, media_path_meta("a.mp3", NULL, 8, album, sizeof(album),
                                              title, sizeof(title)));
    TEST_ASSERT_EQUAL_INT(-1, media_path_meta("a.mp3", artist, 0, album, sizeof(album),
                                              title, sizeof(title)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_path_meta_artist_album_track);
    RUN_TEST(test_path_meta_artist_only);
    RUN_TEST(test_path_meta_loose_file);
    RUN_TEST(test_path_meta_deeper_folders);
    RUN_TEST(test_path_meta_truncates);
    RUN_TEST(test_path_meta_rejects_bad_args);
    return UNITY_END();
}
