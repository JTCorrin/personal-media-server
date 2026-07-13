#include "unity.h"

#include "media_server/media/metadata.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <taglib/tag_c.h>

void setUp(void) {}
void tearDown(void) {}

void test_metadata_parses_full_filename(void)
{
    media_metadata_t metadata;

    TEST_ASSERT_EQUAL_INT(
        0, media_metadata_from_path(
               "Artist/Album/Artist - Album - 03 - Track title.flac", &metadata));
    TEST_ASSERT_EQUAL_STRING("Artist", metadata.artist);
    TEST_ASSERT_EQUAL_STRING("Album", metadata.album);
    TEST_ASSERT_EQUAL_STRING("Track title", metadata.title);
    TEST_ASSERT_EQUAL_UINT32(3, metadata.track_number);
}

void test_metadata_parses_common_numbered_filenames(void)
{
    media_metadata_t metadata;

    TEST_ASSERT_EQUAL_INT(
        0, media_metadata_from_path("Artist/Album/03 - Track title.mp3", &metadata));
    TEST_ASSERT_EQUAL_STRING("Track title", metadata.title);
    TEST_ASSERT_EQUAL_UINT32(3, metadata.track_number);

    TEST_ASSERT_EQUAL_INT(
        0, media_metadata_from_path("Artist/Album/04 Track title.mp3", &metadata));
    TEST_ASSERT_EQUAL_STRING("Track title", metadata.title);
    TEST_ASSERT_EQUAL_UINT32(4, metadata.track_number);

    TEST_ASSERT_EQUAL_INT(
        0, media_metadata_from_path("Artist/Album/05. Track title.mp3", &metadata));
    TEST_ASSERT_EQUAL_STRING("Track title", metadata.title);
    TEST_ASSERT_EQUAL_UINT32(5, metadata.track_number);
}

void test_metadata_does_not_strip_unrecognized_filename(void)
{
    media_metadata_t metadata;

    TEST_ASSERT_EQUAL_INT(
        0, media_metadata_from_path("Artist/Album/track01.mp3", &metadata));
    TEST_ASSERT_EQUAL_STRING("track01", metadata.title);
    TEST_ASSERT_EQUAL_UINT32(0, metadata.track_number);
}

static int write_empty_wav(const char *path)
{
    static const unsigned char wav[] = {
        'R', 'I', 'F', 'F', 38, 0, 0, 0, 'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
        0x44, 0xac, 0, 0, 0x88, 0x58, 1, 0, 2, 0, 16, 0,
        'd', 'a', 't', 'a', 2, 0, 0, 0, 0, 0
    };
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }
    if (fwrite(wav, 1, sizeof(wav), file) != sizeof(wav)) {
        fclose(file);
        return -1;
    }
    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

void test_metadata_embedded_tags_override_path_fallback(void)
{
    char dir[] = "/tmp/media-server-metadata-XXXXXX";
    char path[256];
    TagLib_File *file;
    media_metadata_t metadata;

    TEST_ASSERT_NOT_NULL(mkdtemp(dir));
    TEST_ASSERT_TRUE(snprintf(path, sizeof(path), "%s/tagged.wav", dir) > 0);
    TEST_ASSERT_EQUAL_INT(0, write_empty_wav(path));

    file = taglib_file_new(path);
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_TRUE(taglib_file_is_valid(file));
    taglib_property_set(file, "TITLE", "Tagged title");
    taglib_property_set(file, "ARTIST", "Tagged artist");
    taglib_property_set(file, "ALBUM", "Tagged album");
    taglib_property_set(file, "DATE", "2024-06-07");
    taglib_property_set(file, "GENRE", "Jazz");
    taglib_property_set(file, "TRACKNUMBER", "7/12");
    taglib_property_set(file, "DISCNUMBER", "2/2");
    TEST_ASSERT_TRUE(taglib_file_save(file));
    taglib_file_free(file);

    TEST_ASSERT_EQUAL_INT(
        0, media_metadata_read(path, "Path Artist/Path Album/01 Wrong.wav",
                               &metadata));
    TEST_ASSERT_EQUAL_STRING("Tagged title", metadata.title);
    TEST_ASSERT_EQUAL_STRING("Tagged artist", metadata.artist);
    TEST_ASSERT_EQUAL_STRING("Tagged album", metadata.album);
    TEST_ASSERT_EQUAL_STRING("2024-06-07", metadata.release_date);
    TEST_ASSERT_EQUAL_STRING("Jazz", metadata.genre);
    TEST_ASSERT_EQUAL_UINT32(7, metadata.track_number);
    TEST_ASSERT_EQUAL_UINT32(2, metadata.disc_number);

    TEST_ASSERT_EQUAL_INT(0, unlink(path));
    TEST_ASSERT_EQUAL_INT(0, rmdir(dir));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_metadata_parses_full_filename);
    RUN_TEST(test_metadata_parses_common_numbered_filenames);
    RUN_TEST(test_metadata_does_not_strip_unrecognized_filename);
    RUN_TEST(test_metadata_embedded_tags_override_path_fallback);
    return UNITY_END();
}
