#ifndef DATABASE_H
#define DATABASE_H

#include "sqlite3.h"
#include <string>
#include <vector>
#include <ctime>

struct StorageRecord {
    std::string asset;
    int         index_val;
    char        category; // 'E', 'F', or '*' (all)
    std::string date;     // YYYY-MM-DD
    double      size_mb;
    int         file_count;
};

struct WeightRecord {
    std::string asset;
    int         index_val;
    char        category;
    double      avg_mb;
    double      total_mb;
    int         day_count;
};

struct FileRecord {
    std::string path;
    double      size_mb;
    time_t      created_time;
    std::string asset;
    int         index_val;
    char        category;
};

struct DeletionRecord {
    std::string file_path;
    std::string asset;
    double      size_mb;
    std::string reason;
    std::string timestamp;
};

class Database {
public:
    Database();
    ~Database();

    int  open(const std::string& path);
    void close();
    bool is_open() const { return db_ != nullptr; }

    // Schema
    int create_tables();

    // Storage history
    int insert_snapshot(const StorageRecord& rec);
    std::vector<StorageRecord> get_history(int days, const std::string& asset = "",
                                           int index_val = -1, char category = '*');
    double get_total_current_mb();
    std::vector<WeightRecord> get_average_weights(int days = 14);
    int get_history_day_count();

    // Forecast
    int insert_forecast(const std::string& date, double predicted_mb);
    double get_latest_forecast();

    // Deletion log
    int log_deletion(const DeletionRecord& rec);
    std::vector<DeletionRecord> get_deletion_logs(int limit = 100);

    // Configuration
    int set_config(const std::string& key, const std::string& value);
    std::string get_config(const std::string& key, const std::string& default_val = "");

private:
    sqlite3* db_ = nullptr;
    int exec(const char* sql);
};

#endif // DATABASE_H
