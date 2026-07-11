#include "media_server/http/router.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct route {
    char *method;
    char *pattern;
    router_handler_fn handler;
    void *user_data;
    struct route *next;
} route_t;

struct router {
    route_t *head;
    route_t *tail;
};

static bool valid_pattern(const char *pattern)
{
    size_t param_count = 0;
    const char *segment;

    if (pattern == NULL || pattern[0] != '/') {
        return false;
    }

    segment = pattern + 1;
    while (*segment != '\0') {
        const char *end = strchr(segment, '/');
        size_t length = end == NULL ? strlen(segment) : (size_t)(end - segment);

        if (length > 0 && segment[0] == ':') {
            size_t name_length = length - 1;
            if (name_length == 0 || name_length >= ROUTER_PARAM_NAME_LEN) {
                return false;
            }
            if (++param_count > ROUTER_MAX_PARAMS) {
                return false;
            }
        }

        if (end == NULL) {
            break;
        }
        segment = end + 1;
    }

    return true;
}

static bool route_exists(const router_t *router, const char *method, const char *pattern)
{
    const route_t *route;

    for (route = router->head; route != NULL; route = route->next) {
        if (strcmp(route->method, method) == 0 &&
            strcmp(route->pattern, pattern) == 0) {
            return true;
        }
    }

    return false;
}

static bool match_pattern(const char *pattern, const char *path, router_match_t *match)
{
    const char *pattern_segment = pattern + 1;
    const char *path_segment = path + 1;

    while (true) {
        const char *pattern_end = strchr(pattern_segment, '/');
        const char *path_end = strchr(path_segment, '/');
        size_t pattern_length =
            pattern_end == NULL ? strlen(pattern_segment)
                                : (size_t)(pattern_end - pattern_segment);
        size_t path_length =
            path_end == NULL ? strlen(path_segment) : (size_t)(path_end - path_segment);

        if (pattern_length > 0 && pattern_segment[0] == ':') {
            router_param_t *param;
            size_t name_length = pattern_length - 1;

            if (path_length == 0 || path_length >= ROUTER_PARAM_VALUE_LEN ||
                match->param_count >= ROUTER_MAX_PARAMS) {
                return false;
            }

            param = &match->params[match->param_count++];
            memcpy(param->name, pattern_segment + 1, name_length);
            param->name[name_length] = '\0';
            memcpy(param->value, path_segment, path_length);
            param->value[path_length] = '\0';
        } else if (pattern_length != path_length ||
                   memcmp(pattern_segment, path_segment, pattern_length) != 0) {
            return false;
        }

        if (pattern_end == NULL || path_end == NULL) {
            return pattern_end == NULL && path_end == NULL;
        }

        pattern_segment = pattern_end + 1;
        path_segment = path_end + 1;
    }
}

router_t *router_create(void)
{
    return calloc(1, sizeof(router_t));
}

void router_destroy(router_t *router)
{
    route_t *route;

    if (router == NULL) {
        return;
    }

    route = router->head;
    while (route != NULL) {
        route_t *next = route->next;
        free(route->method);
        free(route->pattern);
        free(route);
        route = next;
    }

    free(router);
}

int router_add(router_t *router, const char *method, const char *pattern,
               router_handler_fn handler, void *user_data)
{
    route_t *route;

    if (router == NULL || method == NULL || method[0] == '\0' || handler == NULL ||
        !valid_pattern(pattern)) {
        return -1;
    }

    if (route_exists(router, method, pattern)) {
        return -1;
    }

    route = calloc(1, sizeof(*route));
    if (route == NULL) {
        return -1;
    }

    /**
    * Why copy? Caller's strings might be temporaries (e.g. from a config parser buffer that gets reused). 
    * The route must own its own stable copies until router_destroy.
    */
    route->method = strdup(method);
    route->pattern = strdup(pattern);
    if (route->method == NULL || route->pattern == NULL) {
        free(route->method);
        free(route->pattern);
        free(route);
        return -1;
    }

    route->handler = handler;
    route->user_data = user_data;

    if (router->tail == NULL) {
        router->head = route;
    } else {
        router->tail->next = route;
    }
    router->tail = route;

    return 0;
}

int router_match(const router_t *router, const char *method, const char *path,
                 router_match_t *out)
{
    const route_t *route;

    if (router == NULL || method == NULL || path == NULL || path[0] != '/' ||
        out == NULL) {
        return -1;
    }

    for (route = router->head; route != NULL; route = route->next) {
        router_match_t candidate = {0};

        if (strcmp(route->method, method) != 0) {
            continue;
        }

        if (!match_pattern(route->pattern, path, &candidate)) {
            continue;
        }

        candidate.method = route->method;
        candidate.pattern = route->pattern;
        candidate.handler = route->handler;
        candidate.user_data = route->user_data;
        *out = candidate;
        return 0;
    }

    return -1;
}

const char *router_param_get(const router_match_t *match, const char *name)
{
    if (match == NULL || name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < match->param_count; ++i) {
        if (strcmp(match->params[i].name, name) == 0) {
            return match->params[i].value;
        }
    }

    return NULL;
}
