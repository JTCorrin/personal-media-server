#include "media_server/library/catalog_store.h"

#include "media_server/util/log.h"

#include <sqlite3.h>
#include <string.h>

#define CATALOG_STORE_SCHEMA_VERSION 1

static int exec_sql(sqlite3 *db, const char *sql)
{
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        LOG_ERROR("catalog_store", "sql failed: %s (%s)",
                  errmsg != NULL ? errmsg : sqlite3_errstr(rc), sql);
        sqlite3_free(errmsg);
        return -1;
    }
    return 0;
}

static int ensure_schema(sqlite3 *db)
{
    if (exec_sql(db,
                 "CREATE TABLE IF NOT EXISTS meta ("
                 "id INTEGER PRIMARY KEY CHECK (id = 1),"
                 "library_root TEXT NOT NULL,"
                 "next_id INTEGER NOT NULL,"
                 "schema_version INTEGER NOT NULL"
                 ");") != 0) {
        return -1;
    }

    if (exec_sql(db,
                 "CREATE TABLE IF NOT EXISTS items ("
                 "id INTEGER PRIMARY KEY,"
                 "kind INTEGER NOT NULL,"
                 "path TEXT NOT NULL UNIQUE,"
                 "filename TEXT NOT NULL,"
                 "artist TEXT NOT NULL,"
                 "album TEXT NOT NULL,"
                 "title TEXT NOT NULL"
                 ");") != 0) {
        return -1;
    }

    return 0;
}

int catalog_store_load(const char *db_path, const char *library_root, catalog_t *out)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    catalog_t *loaded = NULL;
    const char *stored_root = NULL;
    uint32_t next_id = 1;
    uint32_t max_id = 0;
    int schema_version = 0;
    int rc;

    if (db_path == NULL || db_path[0] == '\0' || library_root == NULL ||
        library_root[0] == '\0' || out == NULL) {
        return -1;
    }

    loaded = catalog_create();
    if (loaded == NULL) {
        return -1;
    }

    rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        goto fail;
    }
    sqlite3_busy_timeout(db, 5000);

    if (ensure_schema(db) != 0) {
        goto fail;
    }

    rc = sqlite3_prepare_v2(
        db, "SELECT library_root, next_id, schema_version FROM meta WHERE id = 1;", -1,
        &stmt, NULL);
    if (rc != SQLITE_OK) {
        goto fail;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        goto fail;
    }

    stored_root = (const char *)sqlite3_column_text(stmt, 0);
    {
        sqlite3_int64 raw_next_id = sqlite3_column_int64(stmt, 1);
        if (raw_next_id <= 0 || raw_next_id > UINT32_MAX) {
            goto fail;
        }
        next_id = (uint32_t)raw_next_id;
    }
    schema_version = sqlite3_column_int(stmt, 2);

    if (stored_root == NULL || strcmp(stored_root, library_root) != 0 ||
        schema_version != CATALOG_STORE_SCHEMA_VERSION) {
        goto fail;
    }

    sqlite3_finalize(stmt);
    stmt = NULL;

    rc = sqlite3_prepare_v2(db,
                            "SELECT id, kind, path, filename, artist, album, title "
                            "FROM items ORDER BY id;",
                            -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        goto fail;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        catalog_item_t item;
        const char *path;
        const char *filename;
        const char *artist;
        const char *album;
        const char *title;

        memset(&item, 0, sizeof(item));
        {
            sqlite3_int64 raw_id = sqlite3_column_int64(stmt, 0);
            int raw_kind = sqlite3_column_int(stmt, 1);

            if (raw_id <= 0 || raw_id >= UINT32_MAX ||
                (raw_kind != MEDIA_KIND_AUDIO && raw_kind != MEDIA_KIND_IMAGE)) {
                goto fail;
            }
            item.id = (uint32_t)raw_id;
            item.kind = (media_kind_t)raw_kind;
        }
        path = (const char *)sqlite3_column_text(stmt, 2);
        filename = (const char *)sqlite3_column_text(stmt, 3);
        artist = (const char *)sqlite3_column_text(stmt, 4);
        album = (const char *)sqlite3_column_text(stmt, 5);
        title = (const char *)sqlite3_column_text(stmt, 6);

        if (path == NULL || filename == NULL || artist == NULL || album == NULL ||
            title == NULL || strlen(path) >= CATALOG_PATH_MAX ||
            strlen(filename) >= CATALOG_FILENAME_MAX ||
            strlen(artist) >= CATALOG_META_MAX || strlen(album) >= CATALOG_META_MAX ||
            strlen(title) >= CATALOG_META_MAX) {
            goto fail;
        }

        memcpy(item.path, path, strlen(path) + 1);
        memcpy(item.filename, filename, strlen(filename) + 1);
        memcpy(item.artist, artist, strlen(artist) + 1);
        memcpy(item.album, album, strlen(album) + 1);
        memcpy(item.title, title, strlen(title) + 1);

        if (catalog_add_item(loaded, &item) != 0) {
            goto fail;
        }
        if (item.id > max_id) {
            max_id = item.id;
        }
    }

    if (rc != SQLITE_DONE) {
        goto fail;
    }

    if (max_id >= next_id) {
        goto fail;
    }

    catalog_set_next_id(loaded, next_id);
    if (catalog_replace(out, loaded) != 0) {
        goto fail;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    catalog_destroy(loaded);
    return 0;

fail:
    sqlite3_finalize(stmt);
    if (db != NULL) {
        sqlite3_close(db);
    }
    catalog_destroy(loaded);
    return -1;
}

int catalog_store_save(const char *db_path, const char *library_root,
                       const catalog_t *catalog)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *ins = NULL;
    int rc;

    if (db_path == NULL || db_path[0] == '\0' || library_root == NULL ||
        library_root[0] == '\0' || catalog == NULL) {
        return -1;
    }

    rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        if (db != NULL) {
            sqlite3_close(db);
        }
        return -1;
    }
    sqlite3_busy_timeout(db, 5000);

    if (ensure_schema(db) != 0) {
        sqlite3_close(db);
        return -1;
    }

    if (exec_sql(db, "PRAGMA journal_mode=WAL;") != 0 ||
        exec_sql(db, "BEGIN IMMEDIATE;") != 0) {
        sqlite3_close(db);
        return -1;
    }

    if (exec_sql(db, "DELETE FROM items;") != 0 || exec_sql(db, "DELETE FROM meta;") != 0) {
        exec_sql(db, "ROLLBACK;");
        sqlite3_close(db);
        return -1;
    }

    rc = sqlite3_prepare_v2(db,
                            "INSERT INTO meta (id, library_root, next_id, schema_version) "
                            "VALUES (1, ?, ?, ?);",
                            -1, &ins, NULL);
    if (rc != SQLITE_OK) {
        exec_sql(db, "ROLLBACK;");
        sqlite3_close(db);
        return -1;
    }

    sqlite3_bind_text(ins, 1, library_root, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(ins, 2, (sqlite3_int64)catalog_next_id(catalog));
    sqlite3_bind_int(ins, 3, CATALOG_STORE_SCHEMA_VERSION);
    if (sqlite3_step(ins) != SQLITE_DONE) {
        sqlite3_finalize(ins);
        exec_sql(db, "ROLLBACK;");
        sqlite3_close(db);
        return -1;
    }
    sqlite3_finalize(ins);
    ins = NULL;

    rc = sqlite3_prepare_v2(db,
                            "INSERT INTO items (id, kind, path, filename, artist, album, "
                            "title) VALUES (?, ?, ?, ?, ?, ?, ?);",
                            -1, &ins, NULL);
    if (rc != SQLITE_OK) {
        exec_sql(db, "ROLLBACK;");
        sqlite3_close(db);
        return -1;
    }

    for (size_t i = 0; i < catalog_count(catalog); i++) {
        const catalog_item_t *item = catalog_get(catalog, i);
        if (item == NULL) {
            continue;
        }

        sqlite3_bind_int64(ins, 1, (sqlite3_int64)item->id);
        sqlite3_bind_int(ins, 2, (int)item->kind);
        sqlite3_bind_text(ins, 3, item->path, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 4, item->filename, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 5, item->artist, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 6, item->album, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 7, item->title, -1, SQLITE_TRANSIENT);

        if (sqlite3_step(ins) != SQLITE_DONE) {
            sqlite3_finalize(ins);
            exec_sql(db, "ROLLBACK;");
            sqlite3_close(db);
            return -1;
        }
        sqlite3_reset(ins);
        sqlite3_clear_bindings(ins);
    }

    sqlite3_finalize(ins);

    if (exec_sql(db, "COMMIT;") != 0) {
        exec_sql(db, "ROLLBACK;");
        sqlite3_close(db);
        return -1;
    }

    sqlite3_close(db);
    return 0;
}
