#include "unity.h"

#include "media_server/media/file.h"

void setUp(void) {}
void tearDown(void) {}

void test_media_content_type_audio(void)
{
    TEST_ASSERT_EQUAL_STRING("audio/mpeg", media_content_type("a.mp3"));
    TEST_ASSERT_EQUAL_STRING("audio/flac", media_content_type("a.FLAC"));
    TEST_ASSERT_EQUAL_STRING("audio/ogg", media_content_type("a.ogg"));
}

void test_media_content_type_image(void)
{
    TEST_ASSERT_EQUAL_STRING("image/jpeg", media_content_type("cover.jpg"));
    TEST_ASSERT_EQUAL_STRING("image/png", media_content_type("cover.png"));
}

void test_media_content_type_unknown_falls_back(void)
{
    TEST_ASSERT_EQUAL_STRING("application/octet-stream", media_content_type("a.xyz"));
    TEST_ASSERT_EQUAL_STRING("application/octet-stream", media_content_type("noext"));
    TEST_ASSERT_EQUAL_STRING("application/octet-stream", media_content_type(NULL));
}

void test_media_resolve_path_joins(void)
{
    char out[256];

    TEST_ASSERT_EQUAL_INT(
        0, media_resolve_path("/library", "Artist/Album/a.mp3", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("/library/Artist/Album/a.mp3", out);
}

void test_media_resolve_path_rejects_traversal(void)
{
    char out[256];

    TEST_ASSERT_EQUAL_INT(
        -1, media_resolve_path("/library", "../etc/passwd", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(
        -1, media_resolve_path("/library", "foo/../../x", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(
        -1, media_resolve_path("/library", "/absolute.mp3", out, sizeof(out)));
}

void test_media_resolve_path_rejects_bad_args(void)
{
    char out[64];

    TEST_ASSERT_EQUAL_INT(-1, media_resolve_path(NULL, "a.mp3", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(-1, media_resolve_path("/lib", NULL, out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(-1, media_resolve_path("/lib", "a.mp3", NULL, 8));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_media_content_type_audio);
    RUN_TEST(test_media_content_type_image);
    RUN_TEST(test_media_content_type_unknown_falls_back);
    RUN_TEST(test_media_resolve_path_joins);
    RUN_TEST(test_media_resolve_path_rejects_traversal);
    RUN_TEST(test_media_resolve_path_rejects_bad_args);
    return UNITY_END();
}
