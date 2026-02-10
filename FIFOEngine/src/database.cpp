#include "database.h"
#include <cstring>
#include <cstdio>

Database::Database() {}
Database::~Database() { close(); }

int Database::open(const std::string& path) {
    if (db_) close();
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) return -1;
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA synchronous=NORMAL");
    exec("PRAGMA foreign_keys=ON");
    return create_tables();
}

void Database::close() {
    if (db_) { sqlite3_close(db_); db_ = nullptr; }
}

int Database::exec(const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

int Database::create_tables() {
    const char* sqls[] = {
        R"(CREATE TABLE IF NOT EXISTS storage_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            asset TEXT NOT NULL,
            index_val INTEGER NOT NULL DEFAULT -1,
            category TEXT NOT NULL DEFAULT '*',
            measurement_date TEXT NOT NULL,
            size_mb REAL NOT NULL,
            file_count INTEGER NOT NULL DEFAULT 0,
            created_at TEXT DEFAULT (datetime('now','localtime'))
        ))",
        R"(CREATE TABLE IF NOT EXISTS storage_forecast (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            forecast_date TEXT NOT NULL,
            predicted_mb REAL NOT NULL,
            created_at TEXT DEFAULT (datetime('now','localtime'))
        ))",
        R"(CREATE TABLE IF NOT EXISTS deletion_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT NOT NULL,
            asset TEXT NOT NULL,
            size_mb REAL NOT NULL,
            reason TEXT NOT NULL DEFAULT 'PREDICTIVE_CLEANUP',
            deleted_at TEXT DEFAULT (datetime('now','localtime'))
        ))",
        R"(CREATE TABLE IF NOT EXISTS scheduler_config (
            id INTEGER PRIMARY KEY CHECK(id = 1),
            schedule_hour INTEGER NOT NULL DEFAULT 3,
            schedule_minute INTEGER NOT NULL DEFAULT 0,
            last_run TEXT,
            is_enabled INTEGER NOT NULL DEFAULT 0
        ))",
        R"(CREATE TABLE IF NOT EXISTS configuration (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        ))",
        "CREATE INDEX IF NOT EXISTS idx_hist_date ON storage_history(measurement_date)",
        "CREATE INDEX IF NOT EXISTS idx_hist_asset ON storage_history(asset, index_val, category)",
        "CREATE INDEX IF NOT EXISTS idx_del_date ON deletion_log(deleted_at)",
        R"(INSERT OR IGNORE INTO scheduler_config(id, schedule_hour, schedule_minute, is_enabled)
           VALUES(1, 3, 0, 0))",
        nullptr
    };
    for (int i = 0; sqls[i]; ++i) {
        if (exec(sqls[i]) != 0) return -1;
    }
    return 0;
}

int Database::insert_snapshot(const StorageRecord& rec) {
    const char* sql = "INSERT INTO storage_history(asset, index_val, category, measurement_date, size_mb, file_count) "
                      "VALUES(?,?,?,?,?,?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, rec.asset.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, rec.index_val);
    char cat[2] = { rec.category, 0 };
    sqlite3_bind_text(stmt, 3, cat, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, rec.date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, rec.size_mb);
    sqlite3_bind_int(stmt, 6, rec.file_count);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

std::vector<StorageRecord> Database::get_history(int days, const std::string& asset,
                                                  int index_val, char category) {
    std::vector<StorageRecord> result;
    std::string sql = "SELECT asset, index_val, category, measurement_date, size_mb, file_count "
                      "FROM storage_history WHERE measurement_date >= date('now','localtime','-" +
                      std::to_string(days) + " days')";
    if (!asset.empty()) sql += " AND asset='" + asset + "'";
    if (index_val >= 0) sql += " AND index_val=" + std::to_string(index_val);
    if (category != '*') { sql += " AND category='"; sql += category; sql += "'"; }
    sql += " ORDER BY measurement_date ASC";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StorageRecord r;
        r.asset = (const char*)sqlite3_column_text(stmt, 0);
        r.index_val = sqlite3_column_int(stmt, 1);
        const char* c = (const char*)sqlite3_column_text(stmt, 2);
        r.category = c ? c[0] : '*';
        r.date = (const char*)sqlite3_column_text(stmt, 3);
        r.size_mb = sqlite3_column_double(stmt, 4);
        r.file_count = sqlite3_column_int(stmt, 5);
        result.push_back(r);
    }
    sqlite3_finalize(stmt);
    return result;
}

double Database::get_total_current_mb() {
    const char* sql = "SELECT COALESCE(SUM(size_mb),0) FROM storage_history "
                      "WHERE measurement_date = date('now','localtime')";
    sqlite3_stmt* stmt = nullptr;
    double total = 0;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            total = sqlite3_column_double(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return total;
}

int Database::insert_forecast(const std::string& date, double predicted_mb) {
    const char* sql = "INSERT INTO storage_forecast(forecast_date, predicted_mb) VALUES(?,?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, predicted_mb);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

double Database::get_latest_forecast() {
    const char* sql = "SELECT predicted_mb FROM storage_forecast ORDER BY id DESC LIMIT 1";
    sqlite3_stmt* stmt = nullptr;
    double val = 0;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            val = sqlite3_column_double(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return val;
}

int Database::log_deletion(const DeletionRecord& rec) {
    const char* sql = "INSERT INTO deletion_log(file_path, asset, size_mb, reason) VALUES(?,?,?,?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, rec.file_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, rec.asset.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, rec.size_mb);
    sqlite3_bind_text(stmt, 4, rec.reason.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

std::vector<DeletionRecord> Database::get_deletion_logs(int limit) {
    std::vector<DeletionRecord> result;
    std::string sql = "SELECT file_path, asset, size_mb, reason, deleted_at FROM deletion_log "
                      "ORDER BY id DESC LIMIT " + std::to_string(limit);
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DeletionRecord r;
        r.file_path = (const char*)sqlite3_column_text(stmt, 0);
        r.asset = (const char*)sqlite3_column_text(stmt, 1);
        r.size_mb = sqlite3_column_double(stmt, 2);
        r.reason = (const char*)sqlite3_column_text(stmt, 3);
        const char* ts = (const char*)sqlite3_column_text(stmt, 4);
        r.timestamp = ts ? ts : "";
        result.push_back(r);
    }
    sqlite3_finalize(stmt);
    return result;
}

int Database::set_config(const std::string& key, const std::string& value) {
    const char* sql = "INSERT OR REPLACE INTO configuration(key, value) VALUES(?,?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

std::string Database::get_config(const std::string& key, const std::string& default_val) {
    const char* sql = "SELECT value FROM configuration WHERE key=?";
    sqlite3_stmt* stmt = nullptr;
    std::string val = default_val;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* v = (const char*)sqlite3_column_text(stmt, 0);
            if (v) val = v;
        }
    }
    sqlite3_finalize(stmt);
    return val;
}

std::vector<WeightRecord> Database::get_average_weights(int days) {
    std::vector<WeightRecord> result;
    std::string sql =
        "SELECT asset, index_val, category, "
        "AVG(size_mb) as avg_mb, SUM(size_mb) as total_mb, "
        "COUNT(DISTINCT measurement_date) as day_count "
        "FROM storage_history "
        "WHERE measurement_date >= date('now','localtime','-" + std::to_string(days) + " days') "
        "GROUP BY asset, index_val, category "
        "ORDER BY asset, index_val, category";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        WeightRecord w;
        const char* a = (const char*)sqlite3_column_text(stmt, 0);
        w.asset = a ? a : "";
        w.index_val = sqlite3_column_int(stmt, 1);
        const char* c = (const char*)sqlite3_column_text(stmt, 2);
        w.category = c ? c[0] : '*';
        w.avg_mb = sqlite3_column_double(stmt, 3);
        w.total_mb = sqlite3_column_double(stmt, 4);
        w.day_count = sqlite3_column_int(stmt, 5);
        result.push_back(w);
    }
    sqlite3_finalize(stmt);
    return result;
}

int Database::get_history_day_count() {
    const char* sql = "SELECT COUNT(DISTINCT measurement_date) FROM storage_history";
    sqlite3_stmt* stmt = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}
