#include "media_server/api/routes.h"
#include "media_server/config.h"
#include "media_server/http/router.h"
#include "media_server/http/server.h"
#include "media_server/library/browse.h"
#include "media_server/library/catalog.h"
#include "media_server/library/catalog_store.h"
#include "media_server/library/runtime.h"
#include "media_server/library/scanner.h"
#include "media_server/library/user_store.h"
#include "media_server/media/kind.h"
#include "media_server/util/log.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

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
    browse_index_t *browse = NULL;
    int exit_code = 1;
    bool runtime_ready = false;
    bool loaded_from_db = false;

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

    if (config.library_dir != NULL && config.catalog_db_path != NULL) {
        if (catalog_store_load(config.catalog_db_path, config.library_dir, catalog) ==
            0) {
            loaded_from_db = true;
            LOG_INFO("main", "loaded catalog snapshot %s: %zu items",
                     config.catalog_db_path, catalog_count(catalog));
        } else {
            LOG_INFO("main", "no usable catalog snapshot at %s; scanning",
                     config.catalog_db_path);
        }
    }

    if (config.library_dir != NULL && !loaded_from_db) {
        if (scanner_scan(config.library_dir, catalog) != 0) {
            LOG_ERROR("main", "failed to scan library: %s", config.library_dir);
            goto cleanup;
        }
        LOG_INFO("main", "scanned %s: %zu items (%zu audio, %zu image)",
                 config.library_dir, catalog_count(catalog),
                 catalog_count_kind(catalog, MEDIA_KIND_AUDIO),
                 catalog_count_kind(catalog, MEDIA_KIND_IMAGE));

        if (config.catalog_db_path != NULL) {
            if (catalog_store_save(config.catalog_db_path, config.library_dir,
                                   catalog) != 0) {
                LOG_WARN("main", "failed to save catalog snapshot: %s",
                         config.catalog_db_path);
            } else {
                LOG_INFO("main", "saved catalog snapshot: %s", config.catalog_db_path);
            }
        }
    }

    /* Initial browse index; background rescans rebuild and swap under lock. */
    browse = browse_index_build(catalog);
    if (browse == NULL) {
        LOG_ERROR("main", "failed to build browse index");
        goto cleanup;
    }
    LOG_INFO("main", "browse index: %zu artists, %zu albums",
             browse_artist_count(browse), browse_album_count(browse));

    app_ctx.catalog = catalog;
    app_ctx.browse = browse;
    app_ctx.library_dir = config.library_dir;
    app_ctx.catalog_db_path = config.catalog_db_path;
    app_ctx.user_db_path = config.user_db_path;
    app_ctx.user_store = NULL;
    catalog = NULL; /* ownership transferred to app_ctx / runtime swap */
    browse = NULL;

    if (config.user_db_path != NULL) {
        app_ctx.user_store = user_store_open(config.user_db_path);
        if (app_ctx.user_store == NULL) {
            LOG_ERROR("main", "failed to open user db: %s", config.user_db_path);
            goto cleanup;
        }
        LOG_INFO("main", "user db ready: %s", config.user_db_path);
    }

    if (library_runtime_init(&app_ctx) != 0) {
        LOG_ERROR("main", "failed to init library runtime");
        goto cleanup;
    }
    runtime_ready = true;

    if (config.library_dir != NULL) {
        app_ctx.last_scan_unix = time(NULL);
        app_ctx.last_scan_ok = true;
    }

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
    if (runtime_ready) {
        library_runtime_shutdown(&app_ctx);
    }
    user_store_close(app_ctx.user_store);
    browse_index_destroy(app_ctx.browse);
    catalog_destroy(app_ctx.catalog);
    browse_index_destroy(browse);
    catalog_destroy(catalog);
    log_shutdown();
    return exit_code;
}
