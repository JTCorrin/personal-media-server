#include "unity.h"

#include "media_server/api/routes.h"
#include "media_server/http/router.h"
#include "media_server/library/catalog.h"

#include <stddef.h>

static router_t *router;
static catalog_t *catalog;
static app_context_t ctx;

/*
 * Playback controls (play/pause/stop/seek) are intentionally not registered:
 * clients own those. Seek is HTTP Range on GET /stream/:id.
 */
typedef struct {
    const char *method;
    const char *path;    /* concrete path to match */
    const char *pattern; /* registered pattern */
} expected_route_t;

static const expected_route_t LIVE_ROUTES[] = {
    {"GET", "/api/ping", "/api/ping"},
    {"GET", "/api/tracks", "/api/tracks"},
    {"GET", "/api/tracks/1", "/api/tracks/:id"},
    {"GET", "/api/images", "/api/images"},
    {"GET", "/api/images/1", "/api/images/:id"},
    {"GET", "/stream/1", "/stream/:id"},
    {"GET", "/cover/3", "/cover/:id"},
};

static const expected_route_t STUB_ROUTES[] = {
    {"GET", "/api/artists", "/api/artists"},
    {"GET", "/api/artists/1", "/api/artists/:id"},
    {"GET", "/api/artists/1/albums", "/api/artists/:id/albums"},
    {"GET", "/api/albums", "/api/albums"},
    {"GET", "/api/albums/1", "/api/albums/:id"},
    {"GET", "/api/albums/1/tracks", "/api/albums/:id/tracks"},
    {"GET", "/api/search", "/api/search"},
    {"POST", "/api/library/scan", "/api/library/scan"},
    {"GET", "/api/library/status", "/api/library/status"},
    {"GET", "/api/playlists", "/api/playlists"},
    {"POST", "/api/playlists", "/api/playlists"},
    {"GET", "/api/playlists/1", "/api/playlists/:id"},
    {"PATCH", "/api/playlists/1", "/api/playlists/:id"},
    {"DELETE", "/api/playlists/1", "/api/playlists/:id"},
    {"GET", "/api/playlists/1/tracks", "/api/playlists/:id/tracks"},
    {"PUT", "/api/playlists/1/tracks", "/api/playlists/:id/tracks"},
};

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

static void assert_route_matches(const expected_route_t *route)
{
    router_match_t match;

    TEST_ASSERT_EQUAL_INT(0, router_match(router, route->method, route->path, &match));
    TEST_ASSERT_EQUAL_STRING(route->pattern, match.pattern);
    TEST_ASSERT_NOT_NULL(match.handler);
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

void test_api_routes_register_all_live(void)
{
    TEST_ASSERT_EQUAL_INT(0, api_routes_register(router, &ctx));
    for (size_t i = 0; i < sizeof(LIVE_ROUTES) / sizeof(LIVE_ROUTES[0]); i++) {
        assert_route_matches(&LIVE_ROUTES[i]);
    }
}

void test_api_routes_register_all_stubs(void)
{
    TEST_ASSERT_EQUAL_INT(0, api_routes_register(router, &ctx));
    for (size_t i = 0; i < sizeof(STUB_ROUTES) / sizeof(STUB_ROUTES[0]); i++) {
        assert_route_matches(&STUB_ROUTES[i]);
    }
}

void test_api_routes_tracks_list_not_swallowed_by_id(void)
{
    router_match_t match;

    TEST_ASSERT_EQUAL_INT(0, api_routes_register(router, &ctx));
    TEST_ASSERT_EQUAL_INT(0, router_match(router, "GET", "/api/tracks", &match));
    TEST_ASSERT_EQUAL_STRING("/api/tracks", match.pattern);
    TEST_ASSERT_EQUAL_PTR(&ctx, match.user_data);

    TEST_ASSERT_EQUAL_INT(0, router_match(router, "GET", "/api/tracks/42", &match));
    TEST_ASSERT_EQUAL_STRING("/api/tracks/:id", match.pattern);
    TEST_ASSERT_EQUAL_PTR(&ctx, match.user_data);
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
    RUN_TEST(test_api_routes_register_all_live);
    RUN_TEST(test_api_routes_register_all_stubs);
    RUN_TEST(test_api_routes_tracks_list_not_swallowed_by_id);
    RUN_TEST(test_api_routes_unknown_path_is_unmatched);
    return UNITY_END();
}
