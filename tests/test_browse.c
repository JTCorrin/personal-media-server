#include "unity.h"

#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"
#include "media_server/media/kind.h"

#include <string.h>

static catalog_t *catalog;
static browse_index_t *index;

void setUp(void)
{
    catalog = catalog_create();
    index = NULL;
    TEST_ASSERT_NOT_NULL(catalog);
}

void tearDown(void)
{
    browse_index_destroy(index);
    catalog_destroy(catalog);
}

void test_browse_empty_catalog(void)
{
    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);
    TEST_ASSERT_EQUAL_UINT(0, browse_artist_count(index));
    TEST_ASSERT_EQUAL_UINT(0, browse_album_count(index));
}

void test_browse_groups_artist_and_album(void)
{
    const browse_artist_t *artist;
    const browse_album_t *album;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Artist/Album/track01.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Artist/Album/track02.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_IMAGE, "Artist/Album/cover.jpg"));

    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);

    TEST_ASSERT_EQUAL_UINT(1, browse_artist_count(index));
    TEST_ASSERT_EQUAL_UINT(1, browse_album_count(index));

    artist = browse_artist_get(index, 0);
    TEST_ASSERT_NOT_NULL(artist);
    TEST_ASSERT_EQUAL_UINT(1, artist->id);
    TEST_ASSERT_EQUAL_STRING("Artist", artist->name);
    TEST_ASSERT_EQUAL_UINT(1, artist->album_count);
    TEST_ASSERT_EQUAL_UINT(2, artist->track_count);

    album = browse_album_get(index, 0);
    TEST_ASSERT_NOT_NULL(album);
    TEST_ASSERT_EQUAL_UINT(1, album->id);
    TEST_ASSERT_EQUAL_STRING("Album", album->name);
    TEST_ASSERT_EQUAL_STRING("Artist", album->artist);
    TEST_ASSERT_EQUAL_UINT(1, album->artist_id);
    TEST_ASSERT_EQUAL_UINT(2, album->track_count);
    TEST_ASSERT_EQUAL_UINT(3, album->cover_id); /* cover.jpg is third catalog item */
}

void test_browse_cover_prefers_cover_over_folder(void)
{
    const browse_album_t *album;
    const catalog_item_t *cover;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Artist/Album/t.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_IMAGE, "Artist/Album/folder.jpg"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_IMAGE, "Artist/Album/cover.jpg"));

    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);

    album = browse_album_get(index, 0);
    TEST_ASSERT_NOT_NULL(album);
    cover = catalog_find_id(catalog, album->cover_id);
    TEST_ASSERT_NOT_NULL(cover);
    TEST_ASSERT_EQUAL_STRING("cover.jpg", cover->filename);
}

void test_browse_cover_absent_when_no_image(void)
{
    const browse_album_t *album;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Artist/Album/t.mp3"));

    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);

    album = browse_album_get(index, 0);
    TEST_ASSERT_NOT_NULL(album);
    TEST_ASSERT_EQUAL_UINT(0, album->cover_id);
}

void test_browse_ignores_loose_files_without_album(void)
{
    TEST_ASSERT_EQUAL_INT(0, catalog_add(catalog, MEDIA_KIND_AUDIO, "loose.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "Solo/song.mp3"));

    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);

    /* loose.mp3 has no artist; Solo/song.mp3 has artist but no album */
    TEST_ASSERT_EQUAL_UINT(1, browse_artist_count(index));
    TEST_ASSERT_EQUAL_STRING("Solo", browse_artist_get(index, 0)->name);
    TEST_ASSERT_EQUAL_UINT(1, browse_artist_get(index, 0)->track_count);
    TEST_ASSERT_EQUAL_UINT(0, browse_artist_get(index, 0)->album_count);
    TEST_ASSERT_EQUAL_UINT(0, browse_album_count(index));
}

void test_browse_find_and_owns_item(void)
{
    const browse_album_t *album;
    const catalog_item_t *track;
    const catalog_item_t *other;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "A/One/t1.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "B/Two/t2.mp3"));

    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);

    album = browse_album_find_id(index, 1);
    TEST_ASSERT_NOT_NULL(album);
    TEST_ASSERT_EQUAL_STRING("One", album->name);

    track = catalog_get(catalog, 0);
    other = catalog_get(catalog, 1);
    TEST_ASSERT_TRUE(browse_album_owns_item(album, track));
    TEST_ASSERT_FALSE(browse_album_owns_item(album, other));
    TEST_ASSERT_NULL(browse_artist_find_id(index, 99));
    TEST_ASSERT_NULL(browse_album_find_id(index, 99));
}

void test_browse_track_links_album_and_cover_ids(void)
{
    browse_track_link_t link;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "A/One/t1.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_IMAGE, "A/One/cover.jpg"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "A/Two/t2.mp3"));
    TEST_ASSERT_EQUAL_INT(0, catalog_add(catalog, MEDIA_KIND_AUDIO, "loose.mp3"));

    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);

    TEST_ASSERT_TRUE(browse_track_link_find(index, 1, &link));
    TEST_ASSERT_EQUAL_UINT(1, link.album_id);
    TEST_ASSERT_EQUAL_UINT(2, link.cover_id);

    TEST_ASSERT_TRUE(browse_track_link_find(index, 3, &link));
    TEST_ASSERT_EQUAL_UINT(2, link.album_id);
    TEST_ASSERT_EQUAL_UINT(0, link.cover_id);

    TEST_ASSERT_FALSE(browse_track_link_find(index, 2, &link));
    TEST_ASSERT_FALSE(browse_track_link_find(index, 4, &link));
    TEST_ASSERT_FALSE(browse_track_link_find(index, 999, &link));
}

void test_browse_track_links_follow_metadata_regrouping(void)
{
    metadata_patch_t patch = {0};
    browse_track_link_t link;

    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "A/One/t1.mp3"));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_AUDIO, "B/Two/t2.mp3"));

    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);
    TEST_ASSERT_TRUE(browse_track_link_find(index, 1, &link));
    TEST_ASSERT_EQUAL_UINT(1, link.album_id);
    TEST_ASSERT_TRUE(browse_track_link_find(index, 2, &link));
    TEST_ASSERT_EQUAL_UINT(2, link.album_id);

    browse_index_destroy(index);
    index = NULL;
    patch.set_mask = METADATA_FIELD_ARTIST | METADATA_FIELD_ALBUM;
    strcpy(patch.values.artist, "B");
    strcpy(patch.values.album, "Two");
    TEST_ASSERT_EQUAL_INT(0, catalog_apply_metadata_patch(catalog, 1, &patch));

    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);
    TEST_ASSERT_EQUAL_UINT(1, browse_album_count(index));
    TEST_ASSERT_TRUE(browse_track_link_find(index, 1, &link));
    TEST_ASSERT_EQUAL_UINT(1, link.album_id);
    TEST_ASSERT_TRUE(browse_track_link_find(index, 2, &link));
    TEST_ASSERT_EQUAL_UINT(1, link.album_id);
}

void test_browse_aggregates_common_album_metadata(void)
{
    metadata_patch_t patch = {0};
    const browse_album_t *album;

    TEST_ASSERT_EQUAL_INT(0, catalog_add(catalog, MEDIA_KIND_AUDIO, "A/One/t1.mp3"));
    TEST_ASSERT_EQUAL_INT(0, catalog_add(catalog, MEDIA_KIND_AUDIO, "A/One/t2.mp3"));
    patch.set_mask = METADATA_FIELD_RELEASE_DATE | METADATA_FIELD_GENRE;
    strcpy(patch.values.release_date, "2024");
    strcpy(patch.values.genre, "Rock");
    TEST_ASSERT_EQUAL_INT(0, catalog_apply_metadata_patch(catalog, 1, &patch));
    TEST_ASSERT_EQUAL_INT(0, catalog_apply_metadata_patch(catalog, 2, &patch));

    index = browse_index_build(catalog);
    album = browse_album_get(index, 0);
    TEST_ASSERT_EQUAL_STRING("2024", album->release_date);
    TEST_ASSERT_EQUAL_STRING("Rock", album->genre);

    browse_index_destroy(index);
    index = NULL;
    memset(&patch, 0, sizeof(patch));
    patch.set_mask = METADATA_FIELD_GENRE;
    strcpy(patch.values.genre, "Jazz");
    TEST_ASSERT_EQUAL_INT(0, catalog_apply_metadata_patch(catalog, 2, &patch));
    index = browse_index_build(catalog);
    album = browse_album_get(index, 0);
    TEST_ASSERT_TRUE(album->genre_mixed);
    TEST_ASSERT_EQUAL_STRING("", album->genre);
}

void test_browse_keeps_path_cover_after_album_override(void)
{
    metadata_patch_t patch = {0};
    const browse_album_t *album;

    TEST_ASSERT_EQUAL_INT(0, catalog_add(catalog, MEDIA_KIND_AUDIO, "A/One/t.mp3"));
    TEST_ASSERT_EQUAL_INT(0, catalog_add(catalog, MEDIA_KIND_IMAGE, "A/One/cover.jpg"));
    patch.set_mask = METADATA_FIELD_ALBUM;
    strcpy(patch.values.album, "Renamed");
    TEST_ASSERT_EQUAL_INT(0, catalog_apply_metadata_patch(catalog, 1, &patch));
    index = browse_index_build(catalog);
    album = browse_album_get(index, 0);
    TEST_ASSERT_EQUAL_STRING("Renamed", album->name);
    TEST_ASSERT_EQUAL_UINT(2, album->cover_id);
}

void test_browse_keeps_path_cover_when_tags_define_album(void)
{
    media_metadata_t metadata = {0};
    const browse_album_t *album;

    strcpy(metadata.artist, "Tagged Artist");
    strcpy(metadata.album, "Tagged Album");
    strcpy(metadata.title, "Tagged Track");
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add_metadata(catalog, MEDIA_KIND_AUDIO, "A/One/t.mp3",
                                &metadata));
    TEST_ASSERT_EQUAL_INT(0, catalog_add(catalog, MEDIA_KIND_IMAGE, "A/One/cover.jpg"));

    index = browse_index_build(catalog);
    album = browse_album_get(index, 0);
    TEST_ASSERT_EQUAL_STRING("Tagged Album", album->name);
    TEST_ASSERT_EQUAL_UINT(2, album->cover_id);
}

void test_browse_links_cover_in_one_level_tagged_album_dir(void)
{
    media_metadata_t metadata = {0};
    const browse_album_t *album;

    strcpy(metadata.artist, "Durand Jones & The Indications");
    strcpy(metadata.album, "Flowers");
    strcpy(metadata.title, "I Need The Answer");
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add_metadata(
               catalog, MEDIA_KIND_AUDIO,
               "Durand Jones & The Indications-Flowers-(2025)/04.flac",
               &metadata));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_IMAGE,
                       "Durand Jones & The Indications-Flowers-(2025)/cover.jpg"));

    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);
    album = browse_album_get(index, 0);
    TEST_ASSERT_NOT_NULL(album);
    TEST_ASSERT_EQUAL_STRING("Flowers", album->name);
    TEST_ASSERT_EQUAL_UINT(2, album->cover_id);
}

void test_browse_links_multi_disc_cover_from_common_parent(void)
{
    media_metadata_t metadata = {0};
    const browse_album_t *album;

    strcpy(metadata.artist, "Artist");
    strcpy(metadata.album, "Album");
    strcpy(metadata.title, "Track");
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add_metadata(catalog, MEDIA_KIND_AUDIO,
                                "Artist-Album/CD1/track01.flac", &metadata));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add_metadata(catalog, MEDIA_KIND_AUDIO,
                                "Artist-Album/CD2/track02.flac", &metadata));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_IMAGE, "Artist-Album/cover.jpg"));

    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);
    album = browse_album_get(index, 0);
    TEST_ASSERT_NOT_NULL(album);
    TEST_ASSERT_EQUAL_UINT(3, album->cover_id);
}

void test_browse_does_not_directory_link_shared_album_dir(void)
{
    media_metadata_t first = {0};
    media_metadata_t second = {0};

    strcpy(first.artist, "Artist");
    strcpy(first.album, "First");
    strcpy(first.title, "One");
    strcpy(second.artist, "Artist");
    strcpy(second.album, "Second");
    strcpy(second.title, "Two");
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add_metadata(catalog, MEDIA_KIND_AUDIO, "Shared/one.flac",
                                &first));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add_metadata(catalog, MEDIA_KIND_AUDIO, "Shared/two.flac",
                                &second));
    TEST_ASSERT_EQUAL_INT(
        0, catalog_add(catalog, MEDIA_KIND_IMAGE, "Shared/cover.jpg"));

    index = browse_index_build(catalog);
    TEST_ASSERT_NOT_NULL(index);
    TEST_ASSERT_EQUAL_UINT(2, browse_album_count(index));
    TEST_ASSERT_EQUAL_UINT(0, browse_album_get(index, 0)->cover_id);
    TEST_ASSERT_EQUAL_UINT(0, browse_album_get(index, 1)->cover_id);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_browse_empty_catalog);
    RUN_TEST(test_browse_groups_artist_and_album);
    RUN_TEST(test_browse_cover_prefers_cover_over_folder);
    RUN_TEST(test_browse_cover_absent_when_no_image);
    RUN_TEST(test_browse_ignores_loose_files_without_album);
    RUN_TEST(test_browse_find_and_owns_item);
    RUN_TEST(test_browse_track_links_album_and_cover_ids);
    RUN_TEST(test_browse_track_links_follow_metadata_regrouping);
    RUN_TEST(test_browse_aggregates_common_album_metadata);
    RUN_TEST(test_browse_keeps_path_cover_after_album_override);
    RUN_TEST(test_browse_keeps_path_cover_when_tags_define_album);
    RUN_TEST(test_browse_links_cover_in_one_level_tagged_album_dir);
    RUN_TEST(test_browse_links_multi_disc_cover_from_common_parent);
    RUN_TEST(test_browse_does_not_directory_link_shared_album_dir);
    return UNITY_END();
}
