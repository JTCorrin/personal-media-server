#ifndef MEDIA_SERVER_UTIL_LOG_INTERNAL_H
#define MEDIA_SERVER_UTIL_LOG_INTERNAL_H

#include "media_server/util/log.h"

#include <stddef.h>
#include <time.h>

#define LOG_DEFAULT_SQLITE_TABLE "app_logs"
#define LOG_TIMESTAMP_LEN 32
#define LOG_MESSAGE_MAX 4096

typedef enum log_sink_kind {
    LOG_SINK_TERMINAL = 0,
    LOG_SINK_FILE,
    LOG_SINK_SQLITE,
} log_sink_kind_t;

typedef struct log_record {
    struct timespec ts;
    log_level_t level;
    const char *module;
    char timestamp[LOG_TIMESTAMP_LEN];
    char message[LOG_MESSAGE_MAX];
} log_record_t;

typedef struct log_sink log_sink_t;

struct log_sink {
    log_sink_kind_t kind;
    void (*write)(log_sink_t *sink, const log_record_t *record);
    void (*flush)(log_sink_t *sink);
    void (*close)(log_sink_t *sink);
    void *ctx;
};

void log_format_record(log_record_t *record, log_level_t level, const char *module,
                       const char *fmt, va_list args);

log_sink_t *log_sink_terminal_create(void);
log_sink_t *log_sink_file_create(const char *path);
log_sink_t *log_sink_sqlite_create(const char *db_path, const char *table);

#endif /* MEDIA_SERVER_UTIL_LOG_INTERNAL_H */
