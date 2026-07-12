#include "unity.h"

#include "media_server/api/params.h"

void setUp(void) {}
void tearDown(void) {}

void test_api_page_defaults(void)
{
    api_page_t page;

    api_page_parse(NULL, NULL, &page);
    TEST_ASSERT_EQUAL_UINT(API_PAGE_LIMIT_DEFAULT, page.limit);
    TEST_ASSERT_EQUAL_UINT(0, page.offset);
}

void test_api_page_parses_values(void)
{
    api_page_t page;

    api_page_parse("10", "5", &page);
    TEST_ASSERT_EQUAL_UINT(10, page.limit);
    TEST_ASSERT_EQUAL_UINT(5, page.offset);
}

void test_api_page_clamps_limit(void)
{
    api_page_t page;

    api_page_parse("0", NULL, &page);
    TEST_ASSERT_EQUAL_UINT(1, page.limit);

    api_page_parse("9999", NULL, &page);
    TEST_ASSERT_EQUAL_UINT(API_PAGE_LIMIT_MAX, page.limit);
}

void test_api_page_invalid_falls_back(void)
{
    api_page_t page;

    api_page_parse("nope", "also", &page);
    TEST_ASSERT_EQUAL_UINT(API_PAGE_LIMIT_DEFAULT, page.limit);
    TEST_ASSERT_EQUAL_UINT(0, page.offset);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_api_page_defaults);
    RUN_TEST(test_api_page_parses_values);
    RUN_TEST(test_api_page_clamps_limit);
    RUN_TEST(test_api_page_invalid_falls_back);
    return UNITY_END();
}
