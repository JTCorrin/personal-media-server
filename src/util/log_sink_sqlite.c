#include "util/log_internal.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    sqlite3 *db;
    sqlite3_stmt *insert_stmt;
    char table[128];
} sqlite_sink_ctx_t;

static int ensure_log_table(sqlite3 *db, const char *table)
{
    char sql[512];
    char *errmsg = NULL;
    int rc;

    snprintf(sql, sizeof(sql),
             "CREATE TABLE IF NOT EXISTS %s ("
             "id INTEGER PRIMARY KEY AUTOINCREMENT,"
             "ts TEXT NOT NULL,"
             "level TEXT NOT NULL,"
             "module TEXT NOT NULL,"
             "message TEXT NOT NULL"
             ");",
             table);

    rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "log: failed to create sqlite table '%s': %s\n", table,
                errmsg != NULL ? errmsg : sqlite3_errstr(rc));
        sqlite3_free(errmsg);
        return -1;
    }

    return 0;
}

static void sqlite_write(log_sink_t *sink, const log_record_t *record)
{
    sqlite_sink_ctx_t *ctx = sink->ctx;

    if (ctx == NULL || ctx->db == NULL || ctx->insert_stmt == NULL) {
        return;
    }

    sqlite3_bind_text(ctx->insert_stmt, 1, record->timestamp, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ctx->insert_stmt, 2, log_level_name(record->level), -1, SQLITE_STATIC);
    sqlite3_bind_text(ctx->insert_stmt, 3, record->module, -1, SQLITE_STATIC);
    sqlite3_bind_text(ctx->insert_stmt, 4, record->message, -1, SQLITE_STATIC);

    if (sqlite3_step(ctx->insert_stmt) != SQLITE_DONE) {
        fprintf(stderr, "log: sqlite insert failed: %s\n", sqlite3_errmsg(ctx->db));
    }

    sqlite3_reset(ctx->insert_stmt);
    sqlite3_clear_bindings(ctx->insert_stmt);
}

static void sqlite_flush(log_sink_t *sink)
{
    sqlite_sink_ctx_t *ctx = sink->ctx;

    if (ctx != NULL && ctx->db != NULL) {
        sqlite3_exec(ctx->db, "PRAGMA wal_checkpoint(PASSIVE);", NULL, NULL, NULL);
    }
}

static void sqlite_close(log_sink_t *sink)
{
    sqlite_sink_ctx_t *ctx = sink->ctx;

    if (ctx != NULL) {
        if (ctx->insert_stmt != NULL) {
            sqlite3_finalize(ctx->insert_stmt);
        }
        if (ctx->db != NULL) {
            sqlite3_close(ctx->db);
        }
        free(ctx);
    }

    free(sink);
}

log_sink_t *log_sink_sqlite_create(const char *db_path, const char *table)
{
    log_sink_t *sink;
    sqlite_sink_ctx_t *ctx;
    char insert_sql[512];
    int rc;

    if (db_path == NULL || db_path[0] == '\0') {
        return NULL;
    }

    if (table == NULL || table[0] == '\0') {
        table = LOG_DEFAULT_SQLITE_TABLE;
    }

    sink = calloc(1, sizeof(*sink));
    ctx = calloc(1, sizeof(*ctx));
    if (sink == NULL || ctx == NULL) {
        free(sink);
        free(ctx);
        return NULL;
    }

    snprintf(ctx->table, sizeof(ctx->table), "%s", table);

    rc = sqlite3_open(db_path, &ctx->db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "log: failed to open sqlite sink '%s': %s\n", db_path,
                sqlite3_errmsg(ctx->db));
        sqlite3_close(ctx->db);
        free(ctx);
        free(sink);
        return NULL;
    }

    if (ensure_log_table(ctx->db, ctx->table) != 0) {
        sqlite_close(sink);
        return NULL;
    }

    snprintf(insert_sql, sizeof(insert_sql),
             "INSERT INTO %s (ts, level, module, message) VALUES (?, ?, ?, ?);",
             ctx->table);

    rc = sqlite3_prepare_v2(ctx->db, insert_sql, -1, &ctx->insert_stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "log: failed to prepare sqlite insert: %s\n", sqlite3_errmsg(ctx->db));
        sqlite_close(sink);
        return NULL;
    }

    sink->kind = LOG_SINK_SQLITE;
    sink->write = sqlite_write;
    sink->flush = sqlite_flush;
    sink->close = sqlite_close;
    sink->ctx = ctx;
    return sink;
}
