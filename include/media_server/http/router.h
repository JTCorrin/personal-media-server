#ifndef MEDIA_SERVER_HTTP_ROUTER_H
#define MEDIA_SERVER_HTTP_ROUTER_H

#include <stddef.h>

/*
 * HTTP method + path router.
 *
 * Responsibilities:
 *   - register routes (method, pattern, handler)
 *   - match an incoming method + path
 *   - extract path parameters (e.g. /stream/:id → id="42")
 *
 * Non-responsibilities (left to the HTTP layer later):
 *   - sockets, parsing raw HTTP, writing response bytes
 *   - query strings, headers, body parsing
 */

#define ROUTER_MAX_PARAMS 8
#define ROUTER_PARAM_NAME_LEN 32
#define ROUTER_PARAM_VALUE_LEN 256

/* Opaque router handle — callers never see the internals. */
typedef struct router router_t;
typedef struct router_match router_match_t;

/*
 * Handler signature used by the application.
 *
 * The HTTP server will later pass concrete request/response objects through
 * req and res. Keeping those arguments opaque lets the router remain
 * independent of whichever HTTP library we choose.
 */
typedef void (*router_handler_fn)(const router_match_t *match, void *req, void *res);

/* One captured path parameter, e.g. name="id", value="42". */
typedef struct router_param {
    char name[ROUTER_PARAM_NAME_LEN];
    char value[ROUTER_PARAM_VALUE_LEN];
} router_param_t;

/*
 * Result of a successful match. Owned by the caller of router_match
 * (stack-allocated is fine). Values are copied into this struct so they
 * remain valid after match returns.
 */
struct router_match {
    const char *method; /* points at the registered route's method string */
    const char *pattern; /* points at the registered route's pattern */
    router_param_t params[ROUTER_MAX_PARAMS];
    size_t param_count;
    router_handler_fn handler;
    void *user_data; /* optional context registered with the route */
};

/* Create an empty router. Returns NULL on allocation failure. */
router_t *router_create(void);

/* Free the router and all registered routes. Safe on NULL. */
void router_destroy(router_t *router);

/*
 * Register a route.
 *
 * method:  "GET", "POST", ... (compared case-sensitively; use uppercase)
 * pattern: "/api/ping" or "/stream/:id" or "/cover/:album_id/:size"
 * handler: function to invoke on match
 * user_data: optional opaque pointer passed through on match (may be NULL)
 *
 * Returns 0 on success, -1 on error (NULL args, OOM, duplicate route, too many
 * params in pattern).
 *
 * Patterns:
 *   - exact segments: /api/albums
 *   - params:         :name  (matches one path segment, no '/')
 *   - no trailing-slash normalization yet (keep it simple for TDD)
 */
int router_add(router_t *router, const char *method, const char *pattern,
               router_handler_fn handler, void *user_data);

/*
 * Find the first matching route for method + path.
 *
 * On match: fills *out and returns 0.
 * On no match (your "404" case): returns -1 and leaves *out untouched.
 *
 * Matching rules (v1):
 *   - method must equal the registered method exactly
 *   - path is split on '/' and compared segment-by-segment
 *   - a :param segment captures that path segment into out->params
 *   - first registered match wins
 */
int router_match(const router_t *router, const char *method, const char *path,
                 router_match_t *out);

/*
 * Look up a captured parameter by name.
 * Returns the value string, or NULL if not present.
 */
const char *router_param_get(const router_match_t *match, const char *name);

#endif /* MEDIA_SERVER_HTTP_ROUTER_H */
