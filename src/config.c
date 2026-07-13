#include "media_server/config.h"

#include <stdio.h>
#include <string.h>

void config_init_defaults(app_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->log.level = LOG_INFO;
    config->log.terminal = true;
    config->log.file_path = NULL;
    config->log.sqlite_path = NULL;
    config->log.sqlite_table = NULL;
    config->listen_url = CONFIG_LISTEN_URL_DEFAULT;
    config->library_dir = NULL;
    config->catalog_db_path = NULL;
    config->show_help = false;
}

void config_print_usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s [options]\n"
            "\n"
            "Options:\n"
            "  --listen <url>         listen URL (default: %s)\n"
            "  --library-dir <path>   media library root directory\n"
            "  --catalog-db <path>    sqlite catalog snapshot (stable ids / fast boot)\n"
            "  --log-level <name>     trace|debug|info|warn|error|fatal (default: info)\n"
            "  --no-terminal-log      disable stderr logging\n"
            "  --log-file <path>      also append logs to a file\n"
            "  --log-db <path>        also append logs to a sqlite database\n"
            "  --log-db-table <name>  sqlite table name (default: app_logs)\n"
            "  -h, --help             show this help\n",
            program != NULL ? program : "media-server", CONFIG_LISTEN_URL_DEFAULT);
}

static int require_value(int argc, int i, const char *flag)
{
    if (i + 1 >= argc) {
        fprintf(stderr, "missing value for %s\n", flag);
        return -1;
    }
    return 0;
}

int config_parse_args(int argc, char *argv[], app_config_t *config)
{
    if (config == NULL) {
        return -1;
    }

    config_init_defaults(config);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            config->show_help = true;
            return 0;
        }

        if (strcmp(argv[i], "--no-terminal-log") == 0) {
            config->log.terminal = false;
            continue;
        }

        if (strcmp(argv[i], "--listen") == 0) {
            if (require_value(argc, i, "--listen") != 0) {
                return -1;
            }
            config->listen_url = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "--library-dir") == 0) {
            if (require_value(argc, i, "--library-dir") != 0) {
                return -1;
            }
            config->library_dir = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "--catalog-db") == 0) {
            if (require_value(argc, i, "--catalog-db") != 0) {
                return -1;
            }
            config->catalog_db_path = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "--log-level") == 0) {
            bool ok = false;

            if (require_value(argc, i, "--log-level") != 0) {
                return -1;
            }

            config->log.level = log_level_from_string(argv[++i], &ok);
            if (!ok) {
                fprintf(stderr, "unknown log level: %s\n", argv[i]);
                return -1;
            }
            continue;
        }

        if (strcmp(argv[i], "--log-file") == 0) {
            if (require_value(argc, i, "--log-file") != 0) {
                return -1;
            }
            config->log.file_path = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "--log-db") == 0) {
            if (require_value(argc, i, "--log-db") != 0) {
                return -1;
            }
            config->log.sqlite_path = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "--log-db-table") == 0) {
            if (require_value(argc, i, "--log-db-table") != 0) {
                return -1;
            }
            config->log.sqlite_table = argv[++i];
            continue;
        }

        fprintf(stderr, "unknown argument: %s\n", argv[i]);
        config_print_usage(argv[0]);
        return -1;
    }

    return 0;
}
