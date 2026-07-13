#include "unity.h"

#include "media_server/api/metadata_patch.h"
#include "media_server/http/json_body.h"

#include "mongoose.h"
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_json_get_string_name(void)
{
    char name[64];
    const char *body = "  { \"name\" : \"My Playlist\" } ";

    TEST_ASSERT_EQUAL_INT(
        0, http_json_get_string_field(body, strlen(body), "name", name, sizeof(name)));
    TEST_ASSERT_EQUAL_STRING("My Playlist", name);
}

void test_json_get_u32_track_id(void)
{
    uint32_t id = 0;
    const char *body = "{\"track_id\":42}";

    TEST_ASSERT_EQUAL_INT(
        0, http_json_get_u32_field(body, strlen(body), "track_id", &id));
    TEST_ASSERT_EQUAL_UINT(42, id);
}

void test_json_get_u32_array(void)
{
    uint32_t ids[8];
    size_t count = 0;
    const char *body = "{\"track_ids\":[1, 2, 99]}";

    TEST_ASSERT_EQUAL_INT(0, http_json_get_u32_array(body, strlen(body), "track_ids",
                                                      ids, 8, &count));
    TEST_ASSERT_EQUAL_UINT(3, count);
    TEST_ASSERT_EQUAL_UINT(1, ids[0]);
    TEST_ASSERT_EQUAL_UINT(2, ids[1]);
    TEST_ASSERT_EQUAL_UINT(99, ids[2]);
}

void test_json_get_empty_array(void)
{
    uint32_t ids[4];
    size_t count = 99;
    const char *body = "{\"track_ids\":[]}";

    TEST_ASSERT_EQUAL_INT(0, http_json_get_u32_array(body, strlen(body), "track_ids",
                                                      ids, 4, &count));
    TEST_ASSERT_EQUAL_UINT(0, count);
}

void test_json_rejects_missing(void)
{
    char name[32];
    uint32_t id = 0;
    const char *body = "{\"other\":1}";

    TEST_ASSERT_EQUAL_INT(
        -1, http_json_get_string_field(body, strlen(body), "name", name, sizeof(name)));
    TEST_ASSERT_EQUAL_INT(-1, http_json_get_u32_field(body, strlen(body), "track_id", &id));
}

void test_json_rejects_malformed_key_smuggling(void)
{
    char name[32];
    const char *body = "{\"ignored\":\"\"name\":\"Injected\"\"}";

    TEST_ASSERT_EQUAL_INT(
        -1, http_json_get_string_field(body, strlen(body), "name", name,
                                       sizeof(name)));
}

void test_json_rejects_trailing_garbage_and_overflow(void)
{
    uint32_t id = 0;
    const char *trailing = "{\"track_id\":42} garbage";
    const char *overflow = "{\"track_id\":4294967296}";

    TEST_ASSERT_EQUAL_INT(
        -1, http_json_get_u32_field(trailing, strlen(trailing), "track_id", &id));
    TEST_ASSERT_EQUAL_INT(
        -1, http_json_get_u32_field(overflow, strlen(overflow), "track_id", &id));
}

void test_json_optional_fields_distinguish_absent_value_and_null(void)
{
    char title[32];
    uint32_t number = 0;
    const char *body = "{\"title\":\"Fixed\",\"genre\":null,\"track_number\":7}";

    TEST_ASSERT_EQUAL_INT(
        HTTP_JSON_FIELD_VALUE,
        http_json_get_optional_string(body, strlen(body), "title", title,
                                      sizeof(title)));
    TEST_ASSERT_EQUAL_STRING("Fixed", title);
    TEST_ASSERT_EQUAL_INT(
        HTTP_JSON_FIELD_NULL,
        http_json_get_optional_string(body, strlen(body), "genre", title,
                                      sizeof(title)));
    TEST_ASSERT_EQUAL_INT(
        HTTP_JSON_FIELD_ABSENT,
        http_json_get_optional_string(body, strlen(body), "album", title,
                                      sizeof(title)));
    TEST_ASSERT_EQUAL_INT(
        HTTP_JSON_FIELD_VALUE,
        http_json_get_optional_u32(body, strlen(body), "track_number", &number));
    TEST_ASSERT_EQUAL_UINT(7, number);
    TEST_ASSERT_EQUAL_INT(
        HTTP_JSON_FIELD_ABSENT,
        http_json_get_optional_u32(body, strlen(body), "disc_number", &number));
}

void test_metadata_patch_validates_and_tracks_nulls(void)
{
    struct mg_http_message req = {0};
    metadata_patch_t patch;

    req.body = mg_str("{\"title\":\"Fixed\",\"release_date\":\"2024-02-29\","
                      "\"track_number\":2,\"genre\":null}");
    TEST_ASSERT_EQUAL_INT(0, api_parse_track_metadata_patch(&req, &patch));
    TEST_ASSERT_BITS(METADATA_FIELD_TITLE, METADATA_FIELD_TITLE, patch.set_mask);
    TEST_ASSERT_BITS(METADATA_FIELD_GENRE, METADATA_FIELD_GENRE, patch.clear_mask);
    TEST_ASSERT_EQUAL_UINT(2, patch.values.track_number);

    req.body = mg_str("{\"release_date\":\"2023-02-29\"}");
    TEST_ASSERT_EQUAL_INT(-1, api_parse_track_metadata_patch(&req, &patch));
    req.body = mg_str("{\"track_number\":0}");
    TEST_ASSERT_EQUAL_INT(-1, api_parse_track_metadata_patch(&req, &patch));
    req.body = mg_str("{\"unknown\":\"value\"}");
    TEST_ASSERT_EQUAL_INT(-1, api_parse_track_metadata_patch(&req, &patch));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_json_get_string_name);
    RUN_TEST(test_json_get_u32_track_id);
    RUN_TEST(test_json_get_u32_array);
    RUN_TEST(test_json_get_empty_array);
    RUN_TEST(test_json_rejects_missing);
    RUN_TEST(test_json_rejects_malformed_key_smuggling);
    RUN_TEST(test_json_rejects_trailing_garbage_and_overflow);
    RUN_TEST(test_json_optional_fields_distinguish_absent_value_and_null);
    RUN_TEST(test_metadata_patch_validates_and_tracks_nulls);
    return UNITY_END();
}
