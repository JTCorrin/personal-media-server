#include "unity.h"

#include "media_server/util/string_buf.h"

#include <string.h>

static string_buf_t sb;

void setUp(void)
{
    TEST_ASSERT_EQUAL_INT(0, string_buf_init(&sb, 4));
}

void tearDown(void)
{
    string_buf_free(&sb);
}

void test_string_buf_starts_empty(void)
{
    TEST_ASSERT_EQUAL_STRING("", string_buf_cstr(&sb));
    TEST_ASSERT_EQUAL_UINT(0, sb.len);
}

void test_string_buf_append_grows(void)
{
    TEST_ASSERT_EQUAL_INT(0, string_buf_append(&sb, "hello"));
    TEST_ASSERT_EQUAL_INT(0, string_buf_append(&sb, ", "));
    TEST_ASSERT_EQUAL_INT(0, string_buf_append(&sb, "world"));

    TEST_ASSERT_EQUAL_STRING("hello, world", string_buf_cstr(&sb));
    TEST_ASSERT_EQUAL_UINT(strlen("hello, world"), sb.len);
    TEST_ASSERT_TRUE(sb.cap > sb.len);
}

void test_string_buf_append_char(void)
{
    TEST_ASSERT_EQUAL_INT(0, string_buf_append_char(&sb, '['));
    TEST_ASSERT_EQUAL_INT(0, string_buf_append_char(&sb, ']'));
    TEST_ASSERT_EQUAL_STRING("[]", string_buf_cstr(&sb));
}

void test_string_buf_append_fmt(void)
{
    TEST_ASSERT_EQUAL_INT(0, string_buf_append_fmt(&sb, "id=%u k=%s", 42u, "audio"));
    TEST_ASSERT_EQUAL_STRING("id=42 k=audio", string_buf_cstr(&sb));
}

void test_string_buf_append_fmt_large_grows(void)
{
    char big[300];

    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';

    TEST_ASSERT_EQUAL_INT(0, string_buf_append_fmt(&sb, "<%s>", big));
    TEST_ASSERT_EQUAL_UINT(sizeof(big) - 1 + 2, sb.len);
}

void test_string_buf_json_plain(void)
{
    TEST_ASSERT_EQUAL_INT(0, string_buf_append_json_string(&sb, "track01.mp3"));
    TEST_ASSERT_EQUAL_STRING("\"track01.mp3\"", string_buf_cstr(&sb));
}

void test_string_buf_json_escapes_quotes_and_backslashes(void)
{
    TEST_ASSERT_EQUAL_INT(0, string_buf_append_json_string(&sb, "a\"b\\c"));
    TEST_ASSERT_EQUAL_STRING("\"a\\\"b\\\\c\"", string_buf_cstr(&sb));
}

void test_string_buf_json_escapes_control_chars(void)
{
    TEST_ASSERT_EQUAL_INT(0, string_buf_append_json_string(&sb, "a\nb\t\x01"));
    TEST_ASSERT_EQUAL_STRING("\"a\\nb\\t\\u0001\"", string_buf_cstr(&sb));
}

void test_string_buf_json_passes_utf8_through(void)
{
    /* Multi-byte UTF-8 must not be escaped byte-by-byte. */
    TEST_ASSERT_EQUAL_INT(0, string_buf_append_json_string(&sb, "caf\xc3\xa9"));
    TEST_ASSERT_EQUAL_STRING("\"caf\xc3\xa9\"", string_buf_cstr(&sb));
}

void test_string_buf_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(-1, string_buf_append(&sb, NULL));
    TEST_ASSERT_EQUAL_INT(-1, string_buf_append_json_string(&sb, NULL));
    TEST_ASSERT_EQUAL_INT(-1, string_buf_init(NULL, 8));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_string_buf_starts_empty);
    RUN_TEST(test_string_buf_append_grows);
    RUN_TEST(test_string_buf_append_char);
    RUN_TEST(test_string_buf_append_fmt);
    RUN_TEST(test_string_buf_append_fmt_large_grows);
    RUN_TEST(test_string_buf_json_plain);
    RUN_TEST(test_string_buf_json_escapes_quotes_and_backslashes);
    RUN_TEST(test_string_buf_json_escapes_control_chars);
    RUN_TEST(test_string_buf_json_passes_utf8_through);
    RUN_TEST(test_string_buf_rejects_null);
    return UNITY_END();
}
