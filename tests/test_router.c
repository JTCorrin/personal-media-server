#include "unity.h"
#include "media_server/http/router.h"

static router_t *router;

static void dummy_handler(const router_match_t *match, void *req, void *res)
{
    (void)match;
    (void)req;
    (void)res;
}

static void other_handler(const router_match_t *match, void *req, void *res)
{
    (void)match;
    (void)req;
    (void)res;
}

void setUp(void)
{
    router = router_create();
    TEST_ASSERT_NOT_NULL(router);
}

void tearDown(void)
{
    router_destroy(router);
}

void test_router_matches_exact_method_and_path(void)
{
    router_match_t match;
    int context = 42;

    TEST_ASSERT_EQUAL_INT(
        0, router_add(router, "GET", "/api/ping", dummy_handler, &context));
    TEST_ASSERT_EQUAL_INT(0, router_match(router, "GET", "/api/ping", &match));

    TEST_ASSERT_EQUAL_STRING("GET", match.method);
    TEST_ASSERT_EQUAL_STRING("/api/ping", match.pattern);
    TEST_ASSERT_EQUAL_UINT(0, match.param_count);
    TEST_ASSERT_TRUE(match.handler == dummy_handler);
    TEST_ASSERT_EQUAL_PTR(&context, match.user_data);
}

void test_router_extracts_path_parameter(void)
{
    router_match_t match;

    TEST_ASSERT_EQUAL_INT(
        0, router_add(router, "GET", "/stream/:id", dummy_handler, NULL));
    TEST_ASSERT_EQUAL_INT(0, router_match(router, "GET", "/stream/42", &match));

    TEST_ASSERT_EQUAL_UINT(1, match.param_count);
    TEST_ASSERT_EQUAL_STRING("id", match.params[0].name);
    TEST_ASSERT_EQUAL_STRING("42", router_param_get(&match, "id"));
    TEST_ASSERT_NULL(router_param_get(&match, "missing"));
}

void test_router_extracts_multiple_path_parameters(void)
{
    router_match_t match;

    TEST_ASSERT_EQUAL_INT(
        0, router_add(router, "GET", "/cover/:album_id/:size", dummy_handler, NULL));
    TEST_ASSERT_EQUAL_INT(
        0, router_match(router, "GET", "/cover/abc123/large", &match));

    TEST_ASSERT_EQUAL_UINT(2, match.param_count);
    TEST_ASSERT_EQUAL_STRING("abc123", router_param_get(&match, "album_id"));
    TEST_ASSERT_EQUAL_STRING("large", router_param_get(&match, "size"));
}

void test_router_does_not_match_different_method(void)
{
    router_match_t match;

    TEST_ASSERT_EQUAL_INT(
        0, router_add(router, "GET", "/api/ping", dummy_handler, NULL));
    TEST_ASSERT_EQUAL_INT(-1, router_match(router, "POST", "/api/ping", &match));
}

void test_router_returns_no_match_for_unknown_path(void)
{
    router_match_t match;

    TEST_ASSERT_EQUAL_INT(
        0, router_add(router, "GET", "/api/ping", dummy_handler, NULL));
    TEST_ASSERT_EQUAL_INT(-1, router_match(router, "GET", "/api/albums", &match));
}

void test_router_uses_first_registered_match(void)
{
    router_match_t match;

    TEST_ASSERT_EQUAL_INT(
        0, router_add(router, "GET", "/stream/:id", dummy_handler, NULL));
    TEST_ASSERT_EQUAL_INT(
        0, router_add(router, "GET", "/stream/:track", other_handler, NULL));
    TEST_ASSERT_EQUAL_INT(0, router_add(router, "POST", "/track/:id", dummy_handler, NULL));
    TEST_ASSERT_EQUAL_INT(0, router_match(router, "GET", "/stream/42", &match));
    TEST_ASSERT_TRUE(match.handler == dummy_handler);
}

void test_router_rejects_duplicate_route(void)
{
    router_match_t match;

    TEST_ASSERT_EQUAL_INT(
        0, router_add(router, "GET", "/api/ping", dummy_handler, NULL));
    TEST_ASSERT_EQUAL_INT(
        -1, router_add(router, "GET", "/api/ping", other_handler, NULL));

    TEST_ASSERT_EQUAL_INT(0, router_match(router, "GET", "/api/ping", &match));
    TEST_ASSERT_TRUE(match.handler == dummy_handler);
}

void test_router_allows_same_path_with_different_method(void)
{
    TEST_ASSERT_EQUAL_INT(
        0, router_add(router, "GET", "/api/ping", dummy_handler, NULL));
    TEST_ASSERT_EQUAL_INT(
        0, router_add(router, "POST", "/api/ping", other_handler, NULL));
}

void test_router_rejects_invalid_route(void)
{
    TEST_ASSERT_EQUAL_INT(
        -1, router_add(router, "GET", "missing-leading-slash", dummy_handler, NULL));
    TEST_ASSERT_EQUAL_INT(
        -1, router_add(router, "GET", "/stream/:", dummy_handler, NULL));
    TEST_ASSERT_EQUAL_INT(
        -1, router_add(router, "GET", "/api/ping", NULL, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_router_matches_exact_method_and_path);
    RUN_TEST(test_router_extracts_path_parameter);
    RUN_TEST(test_router_extracts_multiple_path_parameters);
    RUN_TEST(test_router_does_not_match_different_method);
    RUN_TEST(test_router_returns_no_match_for_unknown_path);
    RUN_TEST(test_router_uses_first_registered_match);
    RUN_TEST(test_router_rejects_duplicate_route);
    RUN_TEST(test_router_allows_same_path_with_different_method);
    RUN_TEST(test_router_rejects_invalid_route);
    return UNITY_END();
}
