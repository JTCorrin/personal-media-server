#include "media_server/api/routes.h"
#include "media_server/http/router.h"
#include "media_server/http/server.h"
#include "media_server/util/log.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int signo)
{
    (void)signo;
    g_stop = 1;
}

static void print_usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s [options]\n"
            "\n"
            "Options:\n"
            "  --listen <url>         listen URL (default: http://127.0.0.1:8080)\n"
            "  --log-level <name>     trace|debug|info|warn|error|fatal (default: info)\n"
            "  --no-terminal-log      disable stderr logging\n"
            "  --log-file <path>      also append logs to a file\n"
            "  --log-db <path>        also append logs to a sqlite database\n"
            "  --log-db-table <name>  sqlite table name (default: app_logs)\n"
            "  -h, --help             show this help\n",
            program);
}

int main(int argc, char *argv[])
{
    log_config_t log_config = {
        .level = LOG_INFO,
        .terminal = true,
        .file_path = NULL,
        .sqlite_path = NULL,
        .sqlite_table = NULL,
    };
    const char *listen_url = "http://127.0.0.1:8080";
    router_t *router = NULL;
    http_server_t *server = NULL;
    int exit_code = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }

        if (strcmp(argv[i], "--no-terminal-log") == 0) {
            log_config.terminal = false;
            continue;
        }

        if (strcmp(argv[i], "--listen") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "missing value for --listen\n");
                return 1;
            }
            listen_url = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "--log-level") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "missing value for --log-level\n");
                return 1;
            }

            bool ok = false;
            log_config.level = log_level_from_string(argv[++i], &ok);
            if (!ok) {
                fprintf(stderr, "unknown log level: %s\n", argv[i]);
                return 1;
            }
            continue;
        }

        if (strcmp(argv[i], "--log-file") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "missing value for --log-file\n");
                return 1;
            }
            log_config.file_path = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "--log-db") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "missing value for --log-db\n");
                return 1;
            }
            log_config.sqlite_path = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "--log-db-table") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "missing value for --log-db-table\n");
                return 1;
            }
            log_config.sqlite_table = argv[++i];
            continue;
        }

        fprintf(stderr, "unknown argument: %s\n", argv[i]);
        print_usage(argv[0]);
        return 1;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (log_init(&log_config) != 0) {
        fprintf(stderr, "failed to initialize logging\n");
        return 1;
    }

    router = router_create();
    if (router == NULL) {
        LOG_ERROR("main", "failed to create router");
        goto cleanup;
    }

    if (api_routes_register(router) != 0) {
        goto cleanup;
    }

    http_server_config_t server_config = {
        .listen_url = listen_url,
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
    log_shutdown();
    return exit_code;
}
