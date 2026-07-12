#include "unity.h"

#include "media_server/api/routes.h"
#include "media_server/http/router.h"
#include "media_server/library/catalog.h"

static router_t *router;
static catalog_t *catalog;
static app_context_t ctx;

void setUp(void)
{
    router = router_create();
    catalog = catalog_create();
    TEST_ASSERT_NOT_NULL(router);
    TEST_ASSERT_NOT_NULL(catalog);
    ctx.catalog = catalog;
    ctx.library_dir = "tests/fixtures/library";
}

void tearDown(void)
{
    router_destroy(router);
    catalog_destroy(catalog);
}

void test_api_routes_register_ping(void)
{
    router_match_t match;

    TEST_ASSERT_EQUAL_INT(0, api_routes_register(router, &ctx));
    TEST_ASSERT_EQUAL_INT(0, router_match(router, "GET", "/api/ping", &match));
    TEST_ASSERT_EQUAL_STRING("/api/ping", match.pattern);
    TEST_ASSERT_NOT_NULL(match.handler);
}

void test_api_routes_register_tracks(void)
{
    router_match_t match;

    TEST_ASSERT_EQUAL_INT(0, api_routes_register(router, &ctx));
    TEST_ASSERT_EQUAL_INT(0, router_match(router, "GET", "/api/tracks", &match));
    TEST_ASSERT_EQUAL_PTR(&ctx, match.user_data);
}

void test_api_routes_register_stream_and_cover(void)
{
    router_match_t match;

    TEST_ASSERT_EQUAL_INT(0, api_routes_register(router, &ctx));

    TEST_ASSERT_EQUAL_INT(0, router_match(router, "GET", "/stream/1", &match));
    TEST_ASSERT_EQUAL_STRING("/stream/:id", match.pattern);
    TEST_ASSERT_EQUAL_STRING("1", router_param_get(&match, "id"));
    TEST_ASSERT_EQUAL_PTR(&ctx, match.user_data);

    TEST_ASSERT_EQUAL_INT(0, router_match(router, "GET", "/cover/3", &match));
    TEST_ASSERT_EQUAL_STRING("/cover/:id", match.pattern);
    TEST_ASSERT_EQUAL_STRING("3", router_param_get(&match, "id"));
}

void test_api_routes_unknown_path_is_unmatched(void)
{
    router_match_t match;

    TEST_ASSERT_EQUAL_INT(0, api_routes_register(router, &ctx));
    TEST_ASSERT_EQUAL_INT(-1, router_match(router, "GET", "/api/missing", &match));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_api_routes_register_ping);
    RUN_TEST(test_api_routes_register_tracks);
    RUN_TEST(test_api_routes_register_stream_and_cover);
    RUN_TEST(test_api_routes_unknown_path_is_unmatched);
    return UNITY_END();
}
