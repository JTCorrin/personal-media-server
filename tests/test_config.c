#include "unity.h"

#include "media_server/config.h"

void setUp(void) {}
void tearDown(void) {}

void test_config_defaults(void)
{
    app_config_t config;

    config_init_defaults(&config);

    TEST_ASSERT_EQUAL(LOG_INFO, config.log.level);
    TEST_ASSERT_TRUE(config.log.terminal);
    TEST_ASSERT_NULL(config.log.file_path);
    TEST_ASSERT_EQUAL_STRING(CONFIG_LISTEN_URL_DEFAULT, config.listen_url);
    TEST_ASSERT_NULL(config.library_dir);
    TEST_ASSERT_FALSE(config.show_help);
}

void test_config_parse_listen_and_library_dir(void)
{
    app_config_t config;
    char *argv[] = {
        "media-server",
        "--listen",
        "http://127.0.0.1:9090",
        "--library-dir",
        "/data/music",
        "--log-level",
        "debug",
        "--no-terminal-log",
    };

    TEST_ASSERT_EQUAL_INT(0, config_parse_args(8, argv, &config));
    TEST_ASSERT_EQUAL_STRING("http://127.0.0.1:9090", config.listen_url);
    TEST_ASSERT_EQUAL_STRING("/data/music", config.library_dir);
    TEST_ASSERT_EQUAL(LOG_DEBUG, config.log.level);
    TEST_ASSERT_FALSE(config.log.terminal);
    TEST_ASSERT_FALSE(config.show_help);
}

void test_config_parse_help(void)
{
    app_config_t config;
    char *argv[] = {"media-server", "--help"};

    TEST_ASSERT_EQUAL_INT(0, config_parse_args(2, argv, &config));
    TEST_ASSERT_TRUE(config.show_help);
}

void test_config_parse_unknown_flag_fails(void)
{
    app_config_t config;
    char *argv[] = {"media-server", "--nope"};

    TEST_ASSERT_EQUAL_INT(-1, config_parse_args(2, argv, &config));
}

void test_config_parse_missing_value_fails(void)
{
    app_config_t config;
    char *argv[] = {"media-server", "--listen"};

    TEST_ASSERT_EQUAL_INT(-1, config_parse_args(2, argv, &config));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_config_defaults);
    RUN_TEST(test_config_parse_listen_and_library_dir);
    RUN_TEST(test_config_parse_help);
    RUN_TEST(test_config_parse_unknown_flag_fails);
    RUN_TEST(test_config_parse_missing_value_fails);
    return UNITY_END();
}
