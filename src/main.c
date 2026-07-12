#include "media_server/api/routes.h"
#include "media_server/config.h"
#include "media_server/http/router.h"
#include "media_server/http/server.h"
#include "media_server/library/catalog.h"
#include "media_server/library/scanner.h"
#include "media_server/media/kind.h"
#include "media_server/util/log.h"

#include <signal.h>
#include <stdio.h>

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int signo)
{
    (void)signo;
    g_stop = 1;
}

/*
 * sigaction over signal(): signal() has implementation-defined semantics
 * (handler reset, syscall restart). With sa_flags = 0 blocking calls like
 * poll() are interrupted (EINTR), so the main loop notices g_stop promptly.
 */
static int install_signal_handlers(void)
{
    struct sigaction sa = {0};

    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) != 0 || sigaction(SIGTERM, &sa, NULL) != 0) {
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    app_config_t config;
    app_context_t app_ctx = {0};
    router_t *router = NULL;
    http_server_t *server = NULL;
    catalog_t *catalog = NULL;
    int exit_code = 1;

    if (config_parse_args(argc, argv, &config) != 0) {
        return 1;
    }

    if (config.show_help) {
        config_print_usage(argv[0]);
        return 0;
    }

    if (install_signal_handlers() != 0) {
        fprintf(stderr, "failed to install signal handlers\n");
        return 1;
    }

    if (log_init(&config.log) != 0) {
        fprintf(stderr, "failed to initialize logging\n");
        return 1;
    }

    catalog = catalog_create();
    if (catalog == NULL) {
        LOG_ERROR("main", "failed to create catalog");
        goto cleanup;
    }

    if (config.library_dir != NULL) {
        if (scanner_scan(config.library_dir, catalog) != 0) {
            LOG_ERROR("main", "failed to scan library: %s", config.library_dir);
            goto cleanup;
        }
        LOG_INFO("main", "scanned %s: %zu items (%zu audio, %zu image)",
                 config.library_dir, catalog_count(catalog),
                 catalog_count_kind(catalog, MEDIA_KIND_AUDIO),
                 catalog_count_kind(catalog, MEDIA_KIND_IMAGE));
    }

    app_ctx.catalog = catalog;
    app_ctx.library_dir = config.library_dir;

    router = router_create();
    if (router == NULL) {
        LOG_ERROR("main", "failed to create router");
        goto cleanup;
    }

    if (api_routes_register(router, &app_ctx) != 0) {
        goto cleanup;
    }

    http_server_config_t server_config = {
        .listen_url = config.listen_url,
        .router = router,
    };

    server = http_server_create(&server_config);
    if (server == NULL) {
        goto cleanup;
    }

    LOG_INFO("main", "media-server ready");

    while (!g_stop) {
        http_server_poll(server, 200);
    }

    LOG_INFO("main", "shutting down");
    exit_code = 0;

cleanup:
    http_server_destroy(server);
    router_destroy(router);
    catalog_destroy(catalog);
    log_shutdown();
    return exit_code;
}
