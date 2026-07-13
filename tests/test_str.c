#include "unity.h"

#include "media_server/util/str.h"

#include <stdint.h>

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

void test_str_edit_distance_ci(void)
{
    TEST_ASSERT_EQUAL_UINT(0, str_edit_distance_ci("Radiohead", "radiohead"));
    TEST_ASSERT_EQUAL_UINT(1, str_edit_distance_ci("Radiohead", "Radiohed"));
    TEST_ASSERT_EQUAL_UINT(2, str_edit_distance_ci("Artist", "Artix"));
    TEST_ASSERT_EQUAL_UINT(SIZE_MAX, str_edit_distance_ci(NULL, "a"));
}

void test_str_fuzzy_ci(void)
{
    TEST_ASSERT_TRUE(str_fuzzy_ci("Radiohead", "Radiohed", 2));
    TEST_ASSERT_TRUE(str_fuzzy_ci("The Beatles", "Beatls", 2));
    TEST_ASSERT_FALSE(str_fuzzy_ci("Radiohead", "xyz", 2));
    TEST_ASSERT_FALSE(str_fuzzy_ci("Artist", "zzzzz", 2));
    /* Exact substring still wins without needing distance. */
    TEST_ASSERT_TRUE(str_fuzzy_ci("Pink Floyd", "Floyd", 2));
}

void test_str_match_score_ranks_substring_above_fuzzy(void)
{
    int beatles = str_match_score_ci("The Beatles", "Beat", true);
    int fuzzy_only = str_match_score_ci("Anita Baker", "Beat", true);
    int prefix = str_match_score_ci("Beat Happening", "Beat", true);
    int exact = str_match_score_ci("Beat", "Beat", true);

    TEST_ASSERT_TRUE(beatles > 0);
    TEST_ASSERT_TRUE(fuzzy_only > 0);
    TEST_ASSERT_TRUE(beatles > fuzzy_only);
    TEST_ASSERT_TRUE(prefix > beatles);
    TEST_ASSERT_TRUE(exact > prefix);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_str_contains_ci_basic);
    RUN_TEST(test_str_contains_ci_empty_needle);
    RUN_TEST(test_str_contains_ci_rejects_null);
    RUN_TEST(test_str_edit_distance_ci);
    RUN_TEST(test_str_fuzzy_ci);
    RUN_TEST(test_str_match_score_ranks_substring_above_fuzzy);
    return UNITY_END();
}
