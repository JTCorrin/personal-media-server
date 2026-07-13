#include "unity.h"

#include "media_server/http/json_body.h"

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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_json_get_string_name);
    RUN_TEST(test_json_get_u32_track_id);
    RUN_TEST(test_json_get_u32_array);
    RUN_TEST(test_json_get_empty_array);
    RUN_TEST(test_json_rejects_missing);
    return UNITY_END();
}
