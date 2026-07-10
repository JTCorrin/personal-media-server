#include "util/log_internal.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static struct {
    bool initialized;
    log_level_t min_level;
    log_sink_t **sinks;
    size_t sink_count;
    pthread_mutex_t lock;
} g_log = {
    .initialized = false,
    .min_level = LOG_INFO,
    .sinks = NULL,
    .sink_count = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER,
};

static const char *const LEVEL_NAMES[] = {
    [LOG_TRACE] = "TRACE",
    [LOG_DEBUG] = "DEBUG",
    [LOG_INFO] = "INFO",
    [LOG_WARN] = "WARN",
    [LOG_ERROR] = "ERROR",
    [LOG_FATAL] = "FATAL",
};

const char *log_level_name(log_level_t level)
{
    if (level < LOG_TRACE || level > LOG_FATAL) {
        return "UNKNOWN";
    }
    return LEVEL_NAMES[level];
}

log_level_t log_level_from_string(const char *name, bool *ok)
{
    if (ok != NULL) {
        *ok = true;
    }

    if (name == NULL) {
        if (ok != NULL) {
            *ok = false;
        }
        return LOG_INFO;
    }

    for (log_level_t level = LOG_TRACE; level <= LOG_FATAL; level++) {
        if (strcasecmp(name, LEVEL_NAMES[level]) == 0) {
            return level;
        }
    }

    if (ok != NULL) {
        *ok = false;
    }
    return LOG_INFO;
}

void log_format_record(log_record_t *record, log_level_t level, const char *module,
                       const char *fmt, va_list args)
{
    struct tm local_tm;
    int written;

    clock_gettime(CLOCK_REALTIME, &record->ts);
    localtime_r(&record->ts.tv_sec, &local_tm);

    written = snprintf(record->timestamp, sizeof(record->timestamp),
                         "%04d-%02d-%02dT%02d:%02d:%02d.%03ld",
                         local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday,
                         local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec,
                         record->ts.tv_nsec / 1000000L);
    if (written < 0) {
        record->timestamp[0] = '\0';
    }

    record->level = level;
    record->module = (module != NULL && module[0] != '\0') ? module : "app";

    written = vsnprintf(record->message, sizeof(record->message), fmt, args);
    if (written < 0) {
        record->message[0] = '\0';
    } else if ((size_t)written >= sizeof(record->message)) {
        size_t suffix_len = strlen("...");
        size_t offset = sizeof(record->message) - suffix_len - 1;
        memcpy(record->message + offset, "...", suffix_len);
        record->message[sizeof(record->message) - 1] = '\0';
    }
}

static void terminal_write(log_sink_t *sink, const log_record_t *record)
{
    FILE *out = stderr;
    bool color = isatty(fileno(out));
    const char *reset = color ? "\033[0m" : "";
    const char *level_color = "";

    (void)sink;

    if (color) {
        switch (record->level) {
        case LOG_TRACE:
        case LOG_DEBUG:
            level_color = "\033[2m";
            break;
        case LOG_INFO:
            level_color = "\033[32m";
            break;
        case LOG_WARN:
            level_color = "\033[33m";
            break;
        case LOG_ERROR:
        case LOG_FATAL:
            level_color = "\033[31m";
            break;
        }
    }

    fprintf(out, "%s %s%-5s%s [%s] %s\n", record->timestamp, level_color,
            log_level_name(record->level), reset, record->module, record->message);
    fflush(out);
}

static void terminal_flush(log_sink_t *sink)
{
    (void)sink;
    fflush(stderr);
}

static void terminal_close(log_sink_t *sink)
{
    free(sink);
}

log_sink_t *log_sink_terminal_create(void)
{
    log_sink_t *sink = calloc(1, sizeof(*sink));
    if (sink == NULL) {
        return NULL;
    }

    sink->kind = LOG_SINK_TERMINAL;
    sink->write = terminal_write;
    sink->flush = terminal_flush;
    sink->close = terminal_close;
    return sink;
}

static int append_sink(log_sink_t *sink)
{
    log_sink_t **next = realloc(g_log.sinks, (g_log.sink_count + 1) * sizeof(*next));
    if (next == NULL) {
        if (sink != NULL) {
            sink->close(sink);
        }
        return -1;
    }

    g_log.sinks = next;
    g_log.sinks[g_log.sink_count++] = sink;
    return 0;
}

static void close_all_sinks(void)
{
    for (size_t i = 0; i < g_log.sink_count; i++) {
        log_sink_t *sink = g_log.sinks[i];
        if (sink != NULL) {
            sink->flush(sink);
            sink->close(sink);
        }
    }

    free(g_log.sinks);
    g_log.sinks = NULL;
    g_log.sink_count = 0;
}

int log_init(const log_config_t *config)
{
    if (config == NULL) {
        return -1;
    }

    if (g_log.initialized) {
        log_shutdown();
    }

    g_log.min_level = config->level;

    if (config->terminal) {
        if (append_sink(log_sink_terminal_create()) != 0) {
            close_all_sinks();
            return -1;
        }
    }

    if (config->file_path != NULL && config->file_path[0] != '\0') {
        if (append_sink(log_sink_file_create(config->file_path)) != 0) {
            close_all_sinks();
            return -1;
        }
    }

    if (config->sqlite_path != NULL && config->sqlite_path[0] != '\0') {
        const char *table = config->sqlite_table;
        if (table == NULL || table[0] == '\0') {
            table = LOG_DEFAULT_SQLITE_TABLE;
        }

        if (append_sink(log_sink_sqlite_create(config->sqlite_path, table)) != 0) {
            close_all_sinks();
            return -1;
        }
    }

    if (g_log.sink_count == 0) {
        if (append_sink(log_sink_terminal_create()) != 0) {
            return -1;
        }
    }

    g_log.initialized = true;
    return 0;
}

void log_shutdown(void)
{
    pthread_mutex_lock(&g_log.lock);
    close_all_sinks();
    g_log.initialized = false;
    pthread_mutex_unlock(&g_log.lock);
}

bool log_is_enabled(log_level_t level)
{
    return g_log.initialized && level >= g_log.min_level;
}

void log_vwrite(log_level_t level, const char *module, const char *fmt, va_list args)
{
    log_record_t record;
    va_list args_copy;

    if (!log_is_enabled(level)) {
        return;
    }

    va_copy(args_copy, args);
    log_format_record(&record, level, module, fmt, args_copy);
    va_end(args_copy);

    pthread_mutex_lock(&g_log.lock);
    for (size_t i = 0; i < g_log.sink_count; i++) {
        log_sink_t *sink = g_log.sinks[i];
        if (sink != NULL && sink->write != NULL) {
            sink->write(sink, &record);
        }
    }
    pthread_mutex_unlock(&g_log.lock);
}

void log_write(log_level_t level, const char *module, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_vwrite(level, module, fmt, args);
    va_end(args);
}
