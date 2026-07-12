#include "unity.h"
#include "media_server/util/log.h"
#include "util/log_internal.h"

#include <stdarg.h>
#include <stdbool.h>

void setUp(void) {}
void tearDown(void) { log_shutdown(); }

void test_log_level_from_string_parses_info(void)
{
    bool ok = false;
    TEST_ASSERT_EQUAL(LOG_INFO, log_level_from_string("info", &ok));
    TEST_ASSERT_TRUE(ok);
}

void test_log_level_from_string_is_case_insensitive(void)
{
    bool ok = false;

    TEST_ASSERT_EQUAL(LOG_DEBUG, log_level_from_string("DEBUG", &ok));
    TEST_ASSERT_TRUE(ok);

    ok = false;
    TEST_ASSERT_EQUAL(LOG_WARN, log_level_from_string("warn", &ok));
    TEST_ASSERT_TRUE(ok);
}

void test_log_level_from_string_unknown_sets_ok_false(void)
{
    bool ok = true;

    TEST_ASSERT_EQUAL(LOG_INFO, log_level_from_string("verbose", &ok));
    TEST_ASSERT_FALSE(ok);
}

void test_log_level_from_string_null_sets_ok_false(void)
{
    bool ok = true;

    TEST_ASSERT_EQUAL(LOG_INFO, log_level_from_string(NULL, &ok));
    TEST_ASSERT_FALSE(ok);
}

void test_log_level_name_matches_level(void)
{
    TEST_ASSERT_EQUAL_STRING("TRACE", log_level_name(LOG_TRACE));
    TEST_ASSERT_EQUAL_STRING("DEBUG", log_level_name(LOG_DEBUG));
    TEST_ASSERT_EQUAL_STRING("INFO", log_level_name(LOG_INFO));
    TEST_ASSERT_EQUAL_STRING("WARN", log_level_name(LOG_WARN));
    TEST_ASSERT_EQUAL_STRING("ERROR", log_level_name(LOG_ERROR));
    TEST_ASSERT_EQUAL_STRING("FATAL", log_level_name(LOG_FATAL));
}

void test_log_level_name_returns_unknown_for_invalid_level(void)
{
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", log_level_name((log_level_t)99));
}

void test_log_is_enabled_requires_init(void)
{
    TEST_ASSERT_FALSE(log_is_enabled(LOG_INFO));
}

void test_log_init_enables_configured_level(void)
{
    log_config_t config = {
        .level = LOG_WARN,
        .terminal = true,
        .file_path = NULL,
        .sqlite_path = NULL,
        .sqlite_table = NULL,
    };

    TEST_ASSERT_EQUAL(0, log_init(&config));
    TEST_ASSERT_TRUE(log_is_enabled(LOG_WARN));
    TEST_ASSERT_TRUE(log_is_enabled(LOG_ERROR));
    TEST_ASSERT_FALSE(log_is_enabled(LOG_INFO));
    TEST_ASSERT_FALSE(log_is_enabled(LOG_DEBUG));
}

static void format_record_helper(log_record_t *record, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_format_record(record, LOG_INFO, "test", fmt, args);
    va_end(args);
}

void test_log_format_record_strips_control_chars(void)
{
    log_record_t record;

    /* ANSI escape + newline injection must not survive into the record. */
    format_record_helper(&record, "GET %s", "/\x1b[31mfake\nINFO forged");
    TEST_ASSERT_EQUAL_STRING("GET /?[31mfake?INFO forged", record.message);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_log_level_from_string_parses_info);
    RUN_TEST(test_log_level_from_string_is_case_insensitive);
    RUN_TEST(test_log_level_from_string_unknown_sets_ok_false);
    RUN_TEST(test_log_level_from_string_null_sets_ok_false);
    RUN_TEST(test_log_level_name_matches_level);
    RUN_TEST(test_log_level_name_returns_unknown_for_invalid_level);
    RUN_TEST(test_log_is_enabled_requires_init);
    RUN_TEST(test_log_init_enables_configured_level);
    RUN_TEST(test_log_format_record_strips_control_chars);
    return UNITY_END();
}