#include "media_server/library/user_store.h"

#include "media_server/util/log.h"

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct user_store {
    sqlite3 *db;
};

static int exec_sql(sqlite3 *db, const char *sql)
{
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        LOG_ERROR("user_store", "sql failed: %s (%s)",
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
                 "schema_version INTEGER NOT NULL"
                 ");") != 0) {
        return -1;
    }
    if (exec_sql(db,
                 "CREATE TABLE IF NOT EXISTS playlists ("
                 "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                 "name TEXT NOT NULL,"
                 "created_unix INTEGER NOT NULL,"
                 "updated_unix INTEGER NOT NULL"
                 ");") != 0) {
        return -1;
    }
    if (exec_sql(db,
                 "CREATE TABLE IF NOT EXISTS playlist_tracks ("
                 "playlist_id INTEGER NOT NULL,"
                 "position INTEGER NOT NULL,"
                 "track_id INTEGER NOT NULL,"
                 "PRIMARY KEY (playlist_id, position),"
                 "FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE"
                 ");") != 0) {
        return -1;
    }
    if (exec_sql(db,
                 "CREATE TABLE IF NOT EXISTS favourites ("
                 "track_id INTEGER PRIMARY KEY,"
                 "added_unix INTEGER NOT NULL"
                 ");") != 0) {
        return -1;
    }
    if (exec_sql(db,
                 "CREATE TABLE IF NOT EXISTS play_history ("
                 "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                 "track_id INTEGER NOT NULL,"
                 "played_unix INTEGER NOT NULL"
                 ");") != 0) {
        return -1;
    }

    /* Initialize a new DB, but never silently bless an incompatible schema. */
    {
        sqlite3_stmt *stmt = NULL;
        int rc;

        if (sqlite3_prepare_v2(db, "SELECT schema_version FROM meta WHERE id = 1;",
                               -1, &stmt, NULL) != SQLITE_OK) {
            return -1;
        }
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            int version = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            return version == USER_STORE_SCHEMA_VERSION ? 0 : -1;
        }
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            return -1;
        }

        if (sqlite3_prepare_v2(
                db, "INSERT INTO meta (id, schema_version) VALUES (1, ?);", -1,
                &stmt, NULL) != SQLITE_OK) {
            return -1;
        }
        sqlite3_bind_int(stmt, 1, USER_STORE_SCHEMA_VERSION);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            return -1;
        }
    }
    return 0;
}

user_store_t *user_store_open(const char *db_path)
{
    user_store_t *store;
    int rc;

    if (db_path == NULL || db_path[0] == '\0') {
        return NULL;
    }

    store = calloc(1, sizeof(*store));
    if (store == NULL) {
        return NULL;
    }

    rc = sqlite3_open(db_path, &store->db);
    if (rc != SQLITE_OK) {
        if (store->db != NULL) {
            sqlite3_close(store->db);
        }
        free(store);
        return NULL;
    }
    sqlite3_busy_timeout(store->db, 5000);

    if (exec_sql(store->db, "PRAGMA foreign_keys=ON;") != 0 ||
        exec_sql(store->db, "PRAGMA journal_mode=WAL;") != 0 ||
        ensure_schema(store->db) != 0) {
        sqlite3_close(store->db);
        free(store);
        return NULL;
    }

    return store;
}

void user_store_close(user_store_t *store)
{
    if (store == NULL) {
        return;
    }
    if (store->db != NULL) {
        sqlite3_close(store->db);
    }
    free(store);
}

static int fill_playlist(sqlite3 *db, uint32_t id, user_playlist_t *out)
{
    sqlite3_stmt *stmt = NULL;
    const char *name;

    if (out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));

    if (sqlite3_prepare_v2(db,
                           "SELECT id, name, created_unix, updated_unix, "
                           "(SELECT COUNT(*) FROM playlist_tracks WHERE playlist_id = playlists.id) "
                           "FROM playlists WHERE id = ?;",
                           -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    out->id = (uint32_t)sqlite3_column_int64(stmt, 0);
    name = (const char *)sqlite3_column_text(stmt, 1);
    if (name == NULL || strlen(name) >= USER_STORE_NAME_MAX) {
        sqlite3_finalize(stmt);
        return -1;
    }
    memcpy(out->name, name, strlen(name) + 1);
    out->created_unix = (time_t)sqlite3_column_int64(stmt, 2);
    out->updated_unix = (time_t)sqlite3_column_int64(stmt, 3);
    out->track_count = (size_t)sqlite3_column_int64(stmt, 4);
    sqlite3_finalize(stmt);
    return 0;
}

int user_store_playlist_create(user_store_t *store, const char *name,
                               user_playlist_t *out)
{
    sqlite3_stmt *stmt = NULL;
    time_t now;
    sqlite3_int64 id;

    if (store == NULL || store->db == NULL || name == NULL || name[0] == '\0' ||
        strlen(name) >= USER_STORE_NAME_MAX) {
        return -1;
    }

    now = time(NULL);
    if (sqlite3_prepare_v2(store->db,
                           "INSERT INTO playlists (name, created_unix, updated_unix) "
                           "VALUES (?, ?, ?);",
                           -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)now);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)now);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);

    id = sqlite3_last_insert_rowid(store->db);
    if (id <= 0 || id > UINT32_MAX) {
        return -1;
    }
    return fill_playlist(store->db, (uint32_t)id, out);
}

int user_store_playlist_get(user_store_t *store, uint32_t id, user_playlist_t *out)
{
    if (store == NULL || store->db == NULL || id == 0) {
        return -1;
    }
    return fill_playlist(store->db, id, out);
}

int user_store_playlist_rename(user_store_t *store, uint32_t id, const char *name)
{
    sqlite3_stmt *stmt = NULL;
    time_t now;

    if (store == NULL || store->db == NULL || id == 0 || name == NULL || name[0] == '\0' ||
        strlen(name) >= USER_STORE_NAME_MAX) {
        return -1;
    }

    now = time(NULL);
    if (sqlite3_prepare_v2(store->db,
                           "UPDATE playlists SET name = ?, updated_unix = ? WHERE id = ?;",
                           -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)now);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)id);
    if (sqlite3_step(stmt) != SQLITE_DONE || sqlite3_changes(store->db) != 1) {
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

int user_store_playlist_delete(user_store_t *store, uint32_t id)
{
    sqlite3_stmt *stmt = NULL;

    if (store == NULL || store->db == NULL || id == 0) {
        return -1;
    }

    if (exec_sql(store->db, "BEGIN IMMEDIATE;") != 0) {
        return -1;
    }

    if (sqlite3_prepare_v2(store->db, "DELETE FROM playlist_tracks WHERE playlist_id = ?;",
                           -1, &stmt, NULL) != SQLITE_OK) {
        exec_sql(store->db, "ROLLBACK;");
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        exec_sql(store->db, "ROLLBACK;");
        return -1;
    }
    sqlite3_finalize(stmt);

    if (sqlite3_prepare_v2(store->db, "DELETE FROM playlists WHERE id = ?;", -1, &stmt,
                           NULL) != SQLITE_OK) {
        exec_sql(store->db, "ROLLBACK;");
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);
    if (sqlite3_step(stmt) != SQLITE_DONE || sqlite3_changes(store->db) != 1) {
        sqlite3_finalize(stmt);
        exec_sql(store->db, "ROLLBACK;");
        return -1;
    }
    sqlite3_finalize(stmt);

    if (exec_sql(store->db, "COMMIT;") != 0) {
        exec_sql(store->db, "ROLLBACK;");
        return -1;
    }
    return 0;
}

size_t user_store_playlist_count(user_store_t *store)
{
    sqlite3_stmt *stmt = NULL;
    size_t n = 0;

    if (store == NULL || store->db == NULL) {
        return 0;
    }
    if (sqlite3_prepare_v2(store->db, "SELECT COUNT(*) FROM playlists;", -1, &stmt,
                           NULL) != SQLITE_OK) {
        return 0;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        n = (size_t)sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return n;
}

size_t user_store_playlist_list(user_store_t *store, size_t offset, size_t limit,
                                user_playlist_t *out, size_t max)
{
    sqlite3_stmt *stmt = NULL;
    size_t n = 0;
    size_t take;

    if (store == NULL || store->db == NULL || out == NULL || max == 0 || limit == 0) {
        return 0;
    }
    take = limit < max ? limit : max;

    if (sqlite3_prepare_v2(store->db,
                           "SELECT id FROM playlists ORDER BY id LIMIT ? OFFSET ?;", -1,
                           &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)take);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)offset);

    while (sqlite3_step(stmt) == SQLITE_ROW && n < take) {
        uint32_t id = (uint32_t)sqlite3_column_int64(stmt, 0);
        if (fill_playlist(store->db, id, &out[n]) == 0) {
            n++;
        }
    }
    sqlite3_finalize(stmt);
    return n;
}

int user_store_playlist_set_tracks(user_store_t *store, uint32_t playlist_id,
                                   const uint32_t *track_ids, size_t count)
{
    sqlite3_stmt *del = NULL;
    sqlite3_stmt *ins = NULL;
    sqlite3_stmt *touch = NULL;
    time_t now;

    if (store == NULL || store->db == NULL || playlist_id == 0) {
        return -1;
    }
    if (count > 0 && track_ids == NULL) {
        return -1;
    }

    /* Ensure playlist exists. */
    {
        user_playlist_t tmp;
        if (fill_playlist(store->db, playlist_id, &tmp) != 0) {
            return -1;
        }
    }

    if (exec_sql(store->db, "BEGIN IMMEDIATE;") != 0) {
        return -1;
    }

    if (sqlite3_prepare_v2(store->db, "DELETE FROM playlist_tracks WHERE playlist_id = ?;",
                           -1, &del, NULL) != SQLITE_OK) {
        exec_sql(store->db, "ROLLBACK;");
        return -1;
    }
    sqlite3_bind_int64(del, 1, (sqlite3_int64)playlist_id);
    if (sqlite3_step(del) != SQLITE_DONE) {
        sqlite3_finalize(del);
        exec_sql(store->db, "ROLLBACK;");
        return -1;
    }
    sqlite3_finalize(del);

    if (count > 0) {
        if (sqlite3_prepare_v2(store->db,
                               "INSERT INTO playlist_tracks (playlist_id, position, track_id) "
                               "VALUES (?, ?, ?);",
                               -1, &ins, NULL) != SQLITE_OK) {
            exec_sql(store->db, "ROLLBACK;");
            return -1;
        }
        for (size_t i = 0; i < count; i++) {
            sqlite3_bind_int64(ins, 1, (sqlite3_int64)playlist_id);
            sqlite3_bind_int64(ins, 2, (sqlite3_int64)i);
            sqlite3_bind_int64(ins, 3, (sqlite3_int64)track_ids[i]);
            if (sqlite3_step(ins) != SQLITE_DONE) {
                sqlite3_finalize(ins);
                exec_sql(store->db, "ROLLBACK;");
                return -1;
            }
            sqlite3_reset(ins);
            sqlite3_clear_bindings(ins);
        }
        sqlite3_finalize(ins);
    }

    now = time(NULL);
    if (sqlite3_prepare_v2(store->db, "UPDATE playlists SET updated_unix = ? WHERE id = ?;",
                           -1, &touch, NULL) != SQLITE_OK) {
        exec_sql(store->db, "ROLLBACK;");
        return -1;
    }
    sqlite3_bind_int64(touch, 1, (sqlite3_int64)now);
    sqlite3_bind_int64(touch, 2, (sqlite3_int64)playlist_id);
    if (sqlite3_step(touch) != SQLITE_DONE) {
        sqlite3_finalize(touch);
        exec_sql(store->db, "ROLLBACK;");
        return -1;
    }
    sqlite3_finalize(touch);

    if (exec_sql(store->db, "COMMIT;") != 0) {
        exec_sql(store->db, "ROLLBACK;");
        return -1;
    }
    return 0;
}

size_t user_store_playlist_get_tracks(user_store_t *store, uint32_t playlist_id,
                                      size_t offset, size_t limit, uint32_t *out,
                                      size_t max)
{
    sqlite3_stmt *stmt = NULL;
    size_t n = 0;
    size_t take;

    if (store == NULL || store->db == NULL || playlist_id == 0 || out == NULL ||
        max == 0 || limit == 0) {
        return 0;
    }
    take = limit < max ? limit : max;

    if (sqlite3_prepare_v2(store->db,
                           "SELECT track_id FROM playlist_tracks WHERE playlist_id = ? "
                           "ORDER BY position LIMIT ? OFFSET ?;",
                           -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)playlist_id);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)take);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)offset);

    while (sqlite3_step(stmt) == SQLITE_ROW && n < take) {
        out[n++] = (uint32_t)sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return n;
}

size_t user_store_playlist_track_count(user_store_t *store, uint32_t playlist_id)
{
    sqlite3_stmt *stmt = NULL;
    size_t n = 0;

    if (store == NULL || store->db == NULL || playlist_id == 0) {
        return 0;
    }
    if (sqlite3_prepare_v2(store->db,
                           "SELECT COUNT(*) FROM playlist_tracks WHERE playlist_id = ?;",
                           -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)playlist_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        n = (size_t)sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return n;
}

int user_store_favourite_add(user_store_t *store, uint32_t track_id)
{
    sqlite3_stmt *stmt = NULL;
    time_t now;

    if (store == NULL || store->db == NULL || track_id == 0) {
        return -1;
    }
    now = time(NULL);
    if (sqlite3_prepare_v2(store->db,
                           "INSERT INTO favourites (track_id, added_unix) VALUES (?, ?) "
                           "ON CONFLICT(track_id) DO UPDATE SET added_unix=excluded.added_unix;",
                           -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)track_id);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)now);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

int user_store_favourite_remove(user_store_t *store, uint32_t track_id)
{
    sqlite3_stmt *stmt = NULL;

    if (store == NULL || store->db == NULL || track_id == 0) {
        return -1;
    }
    if (sqlite3_prepare_v2(store->db, "DELETE FROM favourites WHERE track_id = ?;", -1,
                           &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)track_id);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

bool user_store_favourite_has(user_store_t *store, uint32_t track_id)
{
    sqlite3_stmt *stmt = NULL;
    bool has = false;

    if (store == NULL || store->db == NULL || track_id == 0) {
        return false;
    }
    if (sqlite3_prepare_v2(store->db, "SELECT 1 FROM favourites WHERE track_id = ?;", -1,
                           &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)track_id);
    has = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return has;
}

size_t user_store_favourite_count(user_store_t *store)
{
    sqlite3_stmt *stmt = NULL;
    size_t n = 0;

    if (store == NULL || store->db == NULL) {
        return 0;
    }
    if (sqlite3_prepare_v2(store->db, "SELECT COUNT(*) FROM favourites;", -1, &stmt,
                           NULL) != SQLITE_OK) {
        return 0;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        n = (size_t)sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return n;
}

size_t user_store_favourite_list(user_store_t *store, size_t offset, size_t limit,
                                 uint32_t *out, size_t max)
{
    sqlite3_stmt *stmt = NULL;
    size_t n = 0;
    size_t take;

    if (store == NULL || store->db == NULL || out == NULL || max == 0 || limit == 0) {
        return 0;
    }
    take = limit < max ? limit : max;

    if (sqlite3_prepare_v2(store->db,
                           "SELECT track_id FROM favourites ORDER BY added_unix DESC, "
                           "track_id DESC LIMIT ? OFFSET ?;",
                           -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)take);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)offset);
    while (sqlite3_step(stmt) == SQLITE_ROW && n < take) {
        out[n++] = (uint32_t)sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return n;
}

int user_store_history_append(user_store_t *store, uint32_t track_id)
{
    sqlite3_stmt *stmt = NULL;
    time_t now;

    if (store == NULL || store->db == NULL || track_id == 0) {
        return -1;
    }
    now = time(NULL);
    if (sqlite3_prepare_v2(store->db,
                           "INSERT INTO play_history (track_id, played_unix) VALUES (?, ?);",
                           -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)track_id);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)now);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

size_t user_store_history_count(user_store_t *store)
{
    sqlite3_stmt *stmt = NULL;
    size_t n = 0;

    if (store == NULL || store->db == NULL) {
        return 0;
    }
    if (sqlite3_prepare_v2(store->db, "SELECT COUNT(*) FROM play_history;", -1, &stmt,
                           NULL) != SQLITE_OK) {
        return 0;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        n = (size_t)sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return n;
}

size_t user_store_history_list(user_store_t *store, size_t offset, size_t limit,
                               user_history_entry_t *out, size_t max)
{
    sqlite3_stmt *stmt = NULL;
    size_t n = 0;
    size_t take;

    if (store == NULL || store->db == NULL || out == NULL || max == 0 || limit == 0) {
        return 0;
    }
    take = limit < max ? limit : max;

    if (sqlite3_prepare_v2(store->db,
                           "SELECT track_id, played_unix FROM play_history "
                           "ORDER BY played_unix DESC, id DESC LIMIT ? OFFSET ?;",
                           -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)take);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)offset);
    while (sqlite3_step(stmt) == SQLITE_ROW && n < take) {
        out[n].track_id = (uint32_t)sqlite3_column_int64(stmt, 0);
        out[n].played_unix = (time_t)sqlite3_column_int64(stmt, 1);
        n++;
    }
    sqlite3_finalize(stmt);
    return n;
}
