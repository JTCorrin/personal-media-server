#ifndef MEDIA_SERVER_UTIL_LOG_H
#define MEDIA_SERVER_UTIL_LOG_H

#include <stdarg.h>
#include <stdbool.h>

typedef enum log_level {
    LOG_TRACE = 0,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
} log_level_t;

/*
 * Logging configuration. Any sink field left NULL/false is disabled.
 * sqlite_table defaults to "app_logs" when sqlite_path is set.
 */
typedef struct log_config {
    log_level_t level;
    bool terminal;
    const char *file_path;
    const char *sqlite_path;
    const char *sqlite_table;
} log_config_t;

/* Initialize sinks from config. Returns 0 on success. */
int log_init(const log_config_t *config);

/* Flush and close all sinks. Safe to call if not initialized. */
void log_shutdown(void);

bool log_is_enabled(log_level_t level);

void log_write(log_level_t level, const char *module, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

void log_vwrite(log_level_t level, const char *module, const char *fmt, va_list args);

const char *log_level_name(log_level_t level);

/* Returns LOG_INFO and leaves *ok untouched when name is unknown. */
log_level_t log_level_from_string(const char *name, bool *ok);

#define LOG_TRACE(module, ...) log_write(LOG_TRACE, module, __VA_ARGS__)
#define LOG_DEBUG(module, ...) log_write(LOG_DEBUG, module, __VA_ARGS__)
#define LOG_INFO(module, ...)  log_write(LOG_INFO, module, __VA_ARGS__)
#define LOG_WARN(module, ...)  log_write(LOG_WARN, module, __VA_ARGS__)
#define LOG_ERROR(module, ...) log_write(LOG_ERROR, module, __VA_ARGS__)
#define LOG_FATAL(module, ...) log_write(LOG_FATAL, module, __VA_ARGS__)

#endif /* MEDIA_SERVER_UTIL_LOG_H */
