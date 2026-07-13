#ifndef MEDIA_SERVER_CONFIG_H
#define MEDIA_SERVER_CONFIG_H

#include "media_server/util/log.h"

#include <stdbool.h>

#define CONFIG_LISTEN_URL_DEFAULT "http://127.0.0.1:8080"

/*
 * Application configuration.
 *
 * Today: populated from CLI flags.
 * Later: optional simple key=value file (see config/server.config.example).
 * YAML would need an extra library — defer until CLI + plain conf aren't enough.
 */
typedef struct app_config {
    log_config_t log;
    const char *listen_url;
    const char *library_dir;     /* optional library root; NULL if unset */
    const char *catalog_db_path; /* optional SQLite catalog snapshot; NULL if unset */
    bool show_help;
} app_config_t;

/* Fill defaults. Safe to call before config_parse_args. */
void config_init_defaults(app_config_t *config);

/*
 * Parse argv into config (starts from defaults).
 * Returns 0 on success, -1 on error (message already printed to stderr).
 * Sets config->show_help when -h/--help is seen (still returns 0).
 *
 * Pointer fields may alias argv strings; do not free them.
 */
int config_parse_args(int argc, char *argv[], app_config_t *config);

void config_print_usage(const char *program);

#endif /* MEDIA_SERVER_CONFIG_H */
