#include "util/log_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *file;
    char path[1024];
} file_sink_ctx_t;

static void file_write(log_sink_t *sink, const log_record_t *record)
{
    file_sink_ctx_t *ctx = sink->ctx;

    if (ctx == NULL || ctx->file == NULL) {
        return;
    }

    fprintf(ctx->file, "%s %-5s [%s] %s\n", record->timestamp,
            log_level_name(record->level), record->module, record->message);
    fflush(ctx->file);
}

static void file_flush(log_sink_t *sink)
{
    file_sink_ctx_t *ctx = sink->ctx;

    if (ctx != NULL && ctx->file != NULL) {
        fflush(ctx->file);
    }
}

static void file_close(log_sink_t *sink)
{
    file_sink_ctx_t *ctx = sink->ctx;

    if (ctx != NULL) {
        if (ctx->file != NULL) {
            fclose(ctx->file);
        }
        free(ctx);
    }

    free(sink);
}

log_sink_t *log_sink_file_create(const char *path)
{
    log_sink_t *sink;
    file_sink_ctx_t *ctx;

    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    sink = calloc(1, sizeof(*sink));
    ctx = calloc(1, sizeof(*ctx));
    if (sink == NULL || ctx == NULL) {
        free(sink);
        free(ctx);
        return NULL;
    }

    snprintf(ctx->path, sizeof(ctx->path), "%s", path);
    ctx->file = fopen(ctx->path, "a");
    if (ctx->file == NULL) {
        fprintf(stderr, "log: failed to open file sink '%s': %s\n", ctx->path, strerror(errno));
        free(ctx);
        free(sink);
        return NULL;
    }

    sink->kind = LOG_SINK_FILE;
    sink->write = file_write;
    sink->flush = file_flush;
    sink->close = file_close;
    sink->ctx = ctx;
    return sink;
}
