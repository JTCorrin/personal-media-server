#include "unity.h"

#include "media_server/util/str.h"

void setUp(void) {}
void tearDown(void) {}

void test_str_contains_ci_basic(void)
{
    TEST_ASSERT_TRUE(str_contains_ci("Hello World", "world"));
    TEST_ASSERT_TRUE(str_contains_ci("Hello World", "HELLO"));
    TEST_ASSERT_TRUE(str_contains_ci("Artist", "art"));
    TEST_ASSERT_FALSE(str_contains_ci("Artist", "xyz"));
}

void test_str_contains_ci_empty_needle(void)
{
    TEST_ASSERT_TRUE(str_contains_ci("anything", ""));
    TEST_ASSERT_TRUE(str_contains_ci("", ""));
}

void test_str_contains_ci_rejects_null(void)
{
    TEST_ASSERT_FALSE(str_contains_ci(NULL, "a"));
    TEST_ASSERT_FALSE(str_contains_ci("a", NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_str_contains_ci_basic);
    RUN_TEST(test_str_contains_ci_empty_needle);
    RUN_TEST(test_str_contains_ci_rejects_null);
    return UNITY_END();
}
