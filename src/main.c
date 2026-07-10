#include "media_server/util/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s [options]\n"
            "\n"
            "Options:\n"
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
    log_config_t config = {
        .level = LOG_INFO,
        .terminal = true,
        .file_path = NULL,
        .sqlite_path = NULL,
        .sqlite_table = NULL,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }

        if (strcmp(argv[i], "--no-terminal-log") == 0) {
            config.terminal = false;
            continue;
        }

        if (strcmp(argv[i], "--log-level") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "missing value for --log-level\n");
                return 1;
            }

            bool ok = false;
            config.level = log_level_from_string(argv[++i], &ok);
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
            config.file_path = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "--log-db") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "missing value for --log-db\n");
                return 1;
            }
            config.sqlite_path = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "--log-db-table") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "missing value for --log-db-table\n");
                return 1;
            }
            config.sqlite_table = argv[++i];
            continue;
        }

        fprintf(stderr, "unknown argument: %s\n", argv[i]);
        print_usage(argv[0]);
        return 1;
    }

    if (log_init(&config) != 0) {
        fprintf(stderr, "failed to initialize logging\n");
        return 1;
    }

    LOG_INFO("main", "media-server starting");
    LOG_DEBUG("main", "log level is %s and logging to terminal: %s", log_level_name(config.level), config.terminal ? "yes" : "no");
    LOG_WARN("http", "router not wired yet");
    LOG_ERROR("library", "scanner not implemented (expected for now)");

    log_shutdown();
    return 0;
}
