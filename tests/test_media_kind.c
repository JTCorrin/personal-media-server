#include "unity.h"

#include "media_server/media/kind.h"

void setUp(void) {}
void tearDown(void) {}

void test_media_kind_audio_extensions(void)
{
    TEST_ASSERT_EQUAL(MEDIA_KIND_AUDIO, media_kind_from_path("/a/song.mp3"));
    TEST_ASSERT_EQUAL(MEDIA_KIND_AUDIO, media_kind_from_path("track.FLAC"));
    TEST_ASSERT_EQUAL(MEDIA_KIND_AUDIO, media_kind_from_path("x.ogg"));
    TEST_ASSERT_EQUAL(MEDIA_KIND_AUDIO, media_kind_from_path("x.m4a"));
    TEST_ASSERT_EQUAL(MEDIA_KIND_AUDIO, media_kind_from_path("x.wav"));
}

void test_media_kind_image_extensions(void)
{
    TEST_ASSERT_EQUAL(MEDIA_KIND_IMAGE, media_kind_from_path("/a/cover.jpg"));
    TEST_ASSERT_EQUAL(MEDIA_KIND_IMAGE, media_kind_from_path("cover.JPEG"));
    TEST_ASSERT_EQUAL(MEDIA_KIND_IMAGE, media_kind_from_path("a.png"));
    TEST_ASSERT_EQUAL(MEDIA_KIND_IMAGE, media_kind_from_path("a.webp"));
}

void test_media_kind_ignores_non_media(void)
{
    TEST_ASSERT_EQUAL(MEDIA_KIND_NONE, media_kind_from_path("/a/notes.txt"));
    TEST_ASSERT_EQUAL(MEDIA_KIND_NONE, media_kind_from_path("/a/album"));
    TEST_ASSERT_EQUAL(MEDIA_KIND_NONE, media_kind_from_path(NULL));
    TEST_ASSERT_EQUAL(MEDIA_KIND_NONE, media_kind_from_path(""));
}

void test_media_kind_name(void)
{
    TEST_ASSERT_EQUAL_STRING("audio", media_kind_name(MEDIA_KIND_AUDIO));
    TEST_ASSERT_EQUAL_STRING("image", media_kind_name(MEDIA_KIND_IMAGE));
    TEST_ASSERT_EQUAL_STRING("none", media_kind_name(MEDIA_KIND_NONE));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_media_kind_audio_extensions);
    RUN_TEST(test_media_kind_image_extensions);
    RUN_TEST(test_media_kind_ignores_non_media);
    RUN_TEST(test_media_kind_name);
    return UNITY_END();
}
