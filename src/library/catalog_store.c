#include "media_server/library/catalog_store.h"

#include "media_server/util/log.h"

#include <sqlite3.h>
#include <string.h>

#define CATALOG_STORE_SCHEMA_VERSION 2

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

static int column_exists(sqlite3 *db, const char *table, const char *column)
{
    sqlite3_stmt *stmt = NULL;
    char sql[128];
    int found = 0;

    if (sqlite3_snprintf((int)sizeof(sql), sql, "PRAGMA table_info(%w);", table) ==
        NULL) {
        return -1;
    }
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        if (name != NULL && strcmp(name, column) == 0) {
            found = 1;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

static int add_column_if_missing(sqlite3 *db, const char *column,
                                 const char *definition)
{
    char sql[256];
    int exists = column_exists(db, "items", column);

    if (exists < 0) {
        return -1;
    }
    if (exists) {
        return 0;
    }
    if (sqlite3_snprintf((int)sizeof(sql), sql, "ALTER TABLE items ADD COLUMN %w %s;",
                         column, definition) == NULL) {
        return -1;
    }
    return exec_sql(db, sql);
}

static int ensure_schema(sqlite3 *db)
{
    sqlite3_stmt *stmt = NULL;
    int version = 0;
    int rc;

    if (exec_sql(db,
                 "CREATE TABLE IF NOT EXISTS meta ("
                 "id INTEGER PRIMARY KEY CHECK (id = 1),"
                 "library_root TEXT NOT NULL,"
                 "next_id INTEGER NOT NULL,"
                 "schema_version INTEGER NOT NULL"
                 ");") != 0 ||
        exec_sql(db,
                 "CREATE TABLE IF NOT EXISTS items ("
                 "id INTEGER PRIMARY KEY,"
                 "kind INTEGER NOT NULL,"
                 "path TEXT NOT NULL UNIQUE,"
                 "filename TEXT NOT NULL,"
                 "artist TEXT NOT NULL,"
                 "album TEXT NOT NULL,"
                 "title TEXT NOT NULL,"
                 "release_date TEXT NOT NULL DEFAULT '',"
                 "genre TEXT NOT NULL DEFAULT '',"
                 "track_number INTEGER NOT NULL DEFAULT 0,"
                 "disc_number INTEGER NOT NULL DEFAULT 0"
                 ");") != 0) {
        return -1;
    }

    if (sqlite3_prepare_v2(db, "SELECT schema_version FROM meta WHERE id = 1;", -1,
                           &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    } else if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);

    if (version != 0 && version != 1 && version != CATALOG_STORE_SCHEMA_VERSION) {
        return -1;
    }

    if (exec_sql(db, "BEGIN IMMEDIATE;") != 0) {
        return -1;
    }
    if (add_column_if_missing(db, "release_date", "TEXT NOT NULL DEFAULT ''") != 0 ||
        add_column_if_missing(db, "genre", "TEXT NOT NULL DEFAULT ''") != 0 ||
        add_column_if_missing(db, "track_number", "INTEGER NOT NULL DEFAULT 0") != 0 ||
        add_column_if_missing(db, "disc_number", "INTEGER NOT NULL DEFAULT 0") != 0 ||
        exec_sql(db,
                 "CREATE TABLE IF NOT EXISTS metadata_overrides ("
                 "path TEXT PRIMARY KEY,"
                 "title TEXT,"
                 "artist TEXT,"
                 "album TEXT,"
                 "release_date TEXT,"
                 "genre TEXT,"
                 "track_number INTEGER,"
                 "disc_number INTEGER"
                 ");") != 0) {
        exec_sql(db, "ROLLBACK;");
        return -1;
    }
    if (version == 1 &&
        exec_sql(db, "UPDATE meta SET schema_version = 2 WHERE id = 1;") != 0) {
        exec_sql(db, "ROLLBACK;");
        return -1;
    }
    if (exec_sql(db, "COMMIT;") != 0) {
        exec_sql(db, "ROLLBACK;");
        return -1;
    }
    return 0;
}

static int open_store(const char *db_path, sqlite3 **out)
{
    sqlite3 *db = NULL;

    if (db_path == NULL || db_path[0] == '\0' || out == NULL) {
        return -1;
    }
    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
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
    *out = db;
    return 0;
}

static int validate_text(const char *value, size_t max)
{
    return value != NULL && strlen(value) < max;
}

int catalog_store_load(const char *db_path, const char *library_root, catalog_t *out)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    catalog_t *loaded = NULL;
    uint32_t next_id = 1;
    uint32_t max_id = 0;
    int rc;

    if (library_root == NULL || library_root[0] == '\0' || out == NULL ||
        open_store(db_path, &db) != 0) {
        return -1;
    }
    loaded = catalog_create();
    if (loaded == NULL) {
        sqlite3_close(db);
        return -1;
    }

    rc = sqlite3_prepare_v2(
        db, "SELECT library_root, next_id, schema_version FROM meta WHERE id = 1;", -1,
        &stmt, NULL);
    if (rc != SQLITE_OK || sqlite3_step(stmt) != SQLITE_ROW) {
        goto fail;
    }
    {
        const char *stored_root = (const char *)sqlite3_column_text(stmt, 0);
        sqlite3_int64 raw_next = sqlite3_column_int64(stmt, 1);
        int version = sqlite3_column_int(stmt, 2);
        if (stored_root == NULL || strcmp(stored_root, library_root) != 0 ||
            raw_next <= 0 || raw_next > UINT32_MAX ||
            version != CATALOG_STORE_SCHEMA_VERSION) {
            goto fail;
        }
        next_id = (uint32_t)raw_next;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    rc = sqlite3_prepare_v2(
        db,
        "SELECT id, kind, path, filename, artist, album, title, release_date, genre,"
        "track_number, disc_number FROM items ORDER BY id;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        goto fail;
    }
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        catalog_item_t item;
        const char *path = (const char *)sqlite3_column_text(stmt, 2);
        const char *filename = (const char *)sqlite3_column_text(stmt, 3);
        const char *artist = (const char *)sqlite3_column_text(stmt, 4);
        const char *album = (const char *)sqlite3_column_text(stmt, 5);
        const char *title = (const char *)sqlite3_column_text(stmt, 6);
        const char *release_date = (const char *)sqlite3_column_text(stmt, 7);
        const char *genre = (const char *)sqlite3_column_text(stmt, 8);
        sqlite3_int64 raw_id = sqlite3_column_int64(stmt, 0);
        sqlite3_int64 raw_track = sqlite3_column_int64(stmt, 9);
        sqlite3_int64 raw_disc = sqlite3_column_int64(stmt, 10);
        int raw_kind = sqlite3_column_int(stmt, 1);

        memset(&item, 0, sizeof(item));
        if (raw_id <= 0 || raw_id >= UINT32_MAX ||
            (raw_kind != MEDIA_KIND_AUDIO && raw_kind != MEDIA_KIND_IMAGE) ||
            raw_track < 0 || raw_track > UINT32_MAX || raw_disc < 0 ||
            raw_disc > UINT32_MAX || !validate_text(path, CATALOG_PATH_MAX) ||
            !validate_text(filename, CATALOG_FILENAME_MAX) ||
            !validate_text(artist, CATALOG_META_MAX) ||
            !validate_text(album, CATALOG_META_MAX) ||
            !validate_text(title, CATALOG_META_MAX) ||
            !validate_text(release_date, CATALOG_DATE_MAX) ||
            !validate_text(genre, CATALOG_META_MAX)) {
            goto fail;
        }

        item.id = (uint32_t)raw_id;
        item.kind = (media_kind_t)raw_kind;
#define COPY_FIELD(field, src) memcpy(item.field, src, strlen(src) + 1)
        COPY_FIELD(path, path);
        COPY_FIELD(filename, filename);
        COPY_FIELD(artist, artist);
        COPY_FIELD(album, album);
        COPY_FIELD(title, title);
        COPY_FIELD(release_date, release_date);
        COPY_FIELD(genre, genre);
#undef COPY_FIELD
        item.track_number = (uint32_t)raw_track;
        item.disc_number = (uint32_t)raw_disc;
        memcpy(item.scanned.artist, item.artist, sizeof(item.artist));
        memcpy(item.scanned.album, item.album, sizeof(item.album));
        memcpy(item.scanned.title, item.title, sizeof(item.title));
        memcpy(item.scanned.release_date, item.release_date, sizeof(item.release_date));
        memcpy(item.scanned.genre, item.genre, sizeof(item.genre));
        item.scanned.track_number = item.track_number;
        item.scanned.disc_number = item.disc_number;

        if (catalog_add_item(loaded, &item) != 0) {
            goto fail;
        }
        if (item.id > max_id) {
            max_id = item.id;
        }
    }
    if (rc != SQLITE_DONE || max_id >= next_id) {
        goto fail;
    }
    catalog_set_next_id(loaded, next_id);
    sqlite3_finalize(stmt);
    stmt = NULL;
    sqlite3_close(db);
    db = NULL;

    if (catalog_store_apply_overrides(db_path, loaded) != 0 ||
        catalog_replace(out, loaded) != 0) {
        goto fail;
    }
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
    sqlite3_stmt *stmt = NULL;

    if (library_root == NULL || library_root[0] == '\0' || catalog == NULL ||
        open_store(db_path, &db) != 0) {
        return -1;
    }
    if (exec_sql(db, "PRAGMA journal_mode=WAL;") != 0 ||
        exec_sql(db, "BEGIN IMMEDIATE;") != 0) {
        goto fail;
    }
    if (sqlite3_prepare_v2(db, "SELECT library_root FROM meta WHERE id=1;", -1,
                           &stmt, NULL) != SQLITE_OK) {
        goto fail;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *old_root = (const char *)sqlite3_column_text(stmt, 0);
        if (old_root == NULL || strcmp(old_root, library_root) != 0) {
            sqlite3_finalize(stmt);
            stmt = NULL;
            if (exec_sql(db, "DELETE FROM metadata_overrides;") != 0) {
                goto fail;
            }
        }
    }
    sqlite3_finalize(stmt);
    stmt = NULL;
    if (exec_sql(db, "DELETE FROM items;") != 0 ||
        exec_sql(db, "DELETE FROM meta;") != 0) {
        goto fail;
    }

    if (sqlite3_prepare_v2(
            db, "INSERT INTO meta (id, library_root, next_id, schema_version)"
                " VALUES (1, ?, ?, 2);",
            -1, &stmt, NULL) != SQLITE_OK) {
        goto fail;
    }
    sqlite3_bind_text(stmt, 1, library_root, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)catalog_next_id(catalog));
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        goto fail;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (sqlite3_prepare_v2(
            db,
            "INSERT INTO items (id,kind,path,filename,artist,album,title,release_date,"
            "genre,track_number,disc_number) VALUES (?,?,?,?,?,?,?,?,?,?,?);",
            -1, &stmt, NULL) != SQLITE_OK) {
        goto fail;
    }
    for (size_t i = 0; i < catalog_count(catalog); i++) {
        const catalog_item_t *item = catalog_get(catalog, i);
        const media_metadata_t *base;
        if (item == NULL) {
            continue;
        }
        base = &item->scanned;
        sqlite3_bind_int64(stmt, 1, (sqlite3_int64)item->id);
        sqlite3_bind_int(stmt, 2, (int)item->kind);
        sqlite3_bind_text(stmt, 3, item->path, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, item->filename, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, base->artist, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, base->album, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, base->title, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, base->release_date, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 9, base->genre, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 10, (sqlite3_int64)base->track_number);
        sqlite3_bind_int64(stmt, 11, (sqlite3_int64)base->disc_number);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            goto fail;
        }
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    sqlite3_finalize(stmt);
    stmt = NULL;
    if (exec_sql(db, "COMMIT;") != 0) {
        goto fail_no_rollback;
    }
    sqlite3_close(db);
    return 0;

fail:
    sqlite3_finalize(stmt);
    exec_sql(db, "ROLLBACK;");
fail_no_rollback:
    sqlite3_close(db);
    return -1;
}

static void read_override_row(sqlite3_stmt *stmt, metadata_patch_t *override)
{
    memset(override, 0, sizeof(*override));
#define READ_TEXT(col, bit, field)                                                \
    do {                                                                          \
        if (sqlite3_column_type(stmt, col) != SQLITE_NULL) {                      \
            const char *v = (const char *)sqlite3_column_text(stmt, col);         \
            size_t n = strlen(v);                                                  \
            if (n >= sizeof(override->values.field)) {                            \
                n = sizeof(override->values.field) - 1;                           \
            }                                                                     \
            memcpy(override->values.field, v, n);                                 \
            override->values.field[n] = '\0';                                     \
            override->set_mask |= (bit);                                          \
        }                                                                         \
    } while (0)
#define READ_UINT(col, bit, field)                                                \
    do {                                                                          \
        if (sqlite3_column_type(stmt, col) != SQLITE_NULL) {                      \
            override->values.field = (uint32_t)sqlite3_column_int64(stmt, col);   \
            override->set_mask |= (bit);                                          \
        }                                                                         \
    } while (0)
    READ_TEXT(1, METADATA_FIELD_TITLE, title);
    READ_TEXT(2, METADATA_FIELD_ARTIST, artist);
    READ_TEXT(3, METADATA_FIELD_ALBUM, album);
    READ_TEXT(4, METADATA_FIELD_RELEASE_DATE, release_date);
    READ_TEXT(5, METADATA_FIELD_GENRE, genre);
    READ_UINT(6, METADATA_FIELD_TRACK_NUMBER, track_number);
    READ_UINT(7, METADATA_FIELD_DISC_NUMBER, disc_number);
#undef READ_TEXT
#undef READ_UINT
}

int catalog_store_apply_overrides(const char *db_path, catalog_t *catalog)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    int rc;

    if (catalog == NULL || open_store(db_path, &db) != 0 ||
        sqlite3_prepare_v2(
            db, "SELECT path,title,artist,album,release_date,genre,track_number,"
                "disc_number FROM metadata_overrides;",
            -1, &stmt, NULL) != SQLITE_OK) {
        if (db != NULL) {
            sqlite3_close(db);
        }
        return -1;
    }
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char *path = (const char *)sqlite3_column_text(stmt, 0);
        metadata_patch_t override;
        if (path == NULL) {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return -1;
        }
        read_override_row(stmt, &override);
        (void)catalog_apply_metadata_override(catalog, path, &override);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return rc == SQLITE_DONE ? 0 : -1;
}

static int load_override(sqlite3 *db, const char *path, metadata_patch_t *current)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    memset(current, 0, sizeof(*current));
    if (sqlite3_prepare_v2(
            db, "SELECT path,title,artist,album,release_date,genre,track_number,"
                "disc_number FROM metadata_overrides WHERE path=?;",
            -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        read_override_row(stmt, current);
    }
    sqlite3_finalize(stmt);
    return rc == SQLITE_ROW || rc == SQLITE_DONE ? 0 : -1;
}

static void merge_patch(metadata_patch_t *current, const metadata_patch_t *patch)
{
#define MERGE_TEXT(bit, field)                                                    \
    do {                                                                          \
        if ((patch->clear_mask & (bit)) != 0) {                                   \
            current->set_mask &= ~(bit);                                          \
            current->values.field[0] = '\0';                                      \
        } else if ((patch->set_mask & (bit)) != 0) {                              \
            memcpy(current->values.field, patch->values.field,                    \
                   sizeof(current->values.field));                                \
            current->set_mask |= (bit);                                           \
        }                                                                         \
    } while (0)
#define MERGE_UINT(bit, field)                                                    \
    do {                                                                          \
        if ((patch->clear_mask & (bit)) != 0) {                                   \
            current->set_mask &= ~(bit);                                          \
            current->values.field = 0;                                            \
        } else if ((patch->set_mask & (bit)) != 0) {                              \
            current->values.field = patch->values.field;                          \
            current->set_mask |= (bit);                                           \
        }                                                                         \
    } while (0)
    MERGE_TEXT(METADATA_FIELD_TITLE, title);
    MERGE_TEXT(METADATA_FIELD_ARTIST, artist);
    MERGE_TEXT(METADATA_FIELD_ALBUM, album);
    MERGE_TEXT(METADATA_FIELD_RELEASE_DATE, release_date);
    MERGE_TEXT(METADATA_FIELD_GENRE, genre);
    MERGE_UINT(METADATA_FIELD_TRACK_NUMBER, track_number);
    MERGE_UINT(METADATA_FIELD_DISC_NUMBER, disc_number);
#undef MERGE_TEXT
#undef MERGE_UINT
}

static void bind_optional_text(sqlite3_stmt *stmt, int col, uint32_t mask,
                               uint32_t bit, const char *value)
{
    if ((mask & bit) != 0) {
        sqlite3_bind_text(stmt, col, value, -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, col);
    }
}

static void bind_optional_uint(sqlite3_stmt *stmt, int col, uint32_t mask,
                               uint32_t bit, uint32_t value)
{
    if ((mask & bit) != 0) {
        sqlite3_bind_int64(stmt, col, (sqlite3_int64)value);
    } else {
        sqlite3_bind_null(stmt, col);
    }
}

static int save_override(sqlite3 *db, const char *path,
                         const metadata_patch_t *override)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if (override->set_mask == 0) {
        if (sqlite3_prepare_v2(db, "DELETE FROM metadata_overrides WHERE path=?;", -1,
                               &stmt, NULL) != SQLITE_OK) {
            return -1;
        }
        sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
    } else {
        if (sqlite3_prepare_v2(
                db, "INSERT OR REPLACE INTO metadata_overrides "
                    "(path,title,artist,album,release_date,genre,track_number,"
                    "disc_number) VALUES (?,?,?,?,?,?,?,?);",
                -1, &stmt, NULL) != SQLITE_OK) {
            return -1;
        }
        sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
        bind_optional_text(stmt, 2, override->set_mask, METADATA_FIELD_TITLE,
                           override->values.title);
        bind_optional_text(stmt, 3, override->set_mask, METADATA_FIELD_ARTIST,
                           override->values.artist);
        bind_optional_text(stmt, 4, override->set_mask, METADATA_FIELD_ALBUM,
                           override->values.album);
        bind_optional_text(stmt, 5, override->set_mask, METADATA_FIELD_RELEASE_DATE,
                           override->values.release_date);
        bind_optional_text(stmt, 6, override->set_mask, METADATA_FIELD_GENRE,
                           override->values.genre);
        bind_optional_uint(stmt, 7, override->set_mask, METADATA_FIELD_TRACK_NUMBER,
                           override->values.track_number);
        bind_optional_uint(stmt, 8, override->set_mask, METADATA_FIELD_DISC_NUMBER,
                           override->values.disc_number);
    }
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int catalog_store_patch_overrides(const char *db_path, const char *const *paths,
                                  size_t path_count,
                                  const metadata_patch_t *patch)
{
    sqlite3 *db = NULL;

    if (paths == NULL || path_count == 0 || patch == NULL ||
        (patch->set_mask & patch->clear_mask) != 0 ||
        open_store(db_path, &db) != 0 || exec_sql(db, "BEGIN IMMEDIATE;") != 0) {
        if (db != NULL) {
            sqlite3_close(db);
        }
        return -1;
    }
    for (size_t i = 0; i < path_count; i++) {
        metadata_patch_t current;
        if (paths[i] == NULL || load_override(db, paths[i], &current) != 0) {
            goto fail;
        }
        merge_patch(&current, patch);
        if (save_override(db, paths[i], &current) != 0) {
            goto fail;
        }
    }
    if (exec_sql(db, "COMMIT;") != 0) {
        goto fail_no_rollback;
    }
    sqlite3_close(db);
    return 0;

fail:
    exec_sql(db, "ROLLBACK;");
fail_no_rollback:
    sqlite3_close(db);
    return -1;
}
