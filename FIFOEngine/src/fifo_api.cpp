#include "fifo_api.h"
#include "database.h"
#include "scanner.h"
#include "forecast.h"
#include "cleanup.h"
#include "datagen.h"
#include "scheduler.h"
#include <mutex>
#include <cstring>
#include <cstdio>

// Global state
static Database g_db;
static Scheduler g_scheduler;
static std::mutex g_mutex;
static ScanResult g_last_scan;
static ForecastData g_last_forecast;
static std::string g_db_path;

FIFO_API int fifo_init(const char* db_path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_db_path = db_path;
    return g_db.open(db_path);
}

FIFO_API void fifo_shutdown() {
    g_scheduler.stop();
    std::lock_guard<std::mutex> lock(g_mutex);
    g_db.close();
}

FIFO_API int fifo_scan(const char* root_path, int granularity) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_db.is_open()) return FIFO_ERR_DB;

    g_last_scan = scan_directory(root_path, granularity);
    if (g_last_scan.total_files == 0) return FIFO_ERR_NODATA;

    return store_scan_results(g_db, g_last_scan);
}

FIFO_API int fifo_forecast(ForecastResult* out) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_db.is_open()) return FIFO_ERR_DB;

    g_last_forecast = compute_forecast(g_db, g_last_scan.total_mb);
    store_forecast(g_db, g_last_forecast);

    if (out) {
        out->current_mb = g_last_forecast.current_mb;
        out->predicted_mb = g_last_forecast.predicted_mb;
        out->growth_rate_mb_per_day = g_last_forecast.growth_rate;
        out->history_days_available = g_last_forecast.days_available;
    }
    return FIFO_OK;
}

FIFO_API int fifo_evaluate(double limit_mb, EvalResult* out) {
    std::lock_guard<std::mutex> lock(g_mutex);
    double amount = 0;
    int action = evaluate_threshold(g_last_forecast.predicted_mb, limit_mb, &amount);
    if (out) {
        out->action = action;
        out->projected_pct = (limit_mb > 0) ? (g_last_forecast.predicted_mb / limit_mb * 100.0) : 0;
        out->amount_to_delete_mb = amount;
    }
    return FIFO_OK;
}

FIFO_API int fifo_cleanup(double limit_mb, double target_pct, CleanupResult* out) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_db.is_open()) return FIFO_ERR_DB;

    double target_mb = limit_mb * target_pct;
    double amount = g_last_scan.total_mb - target_mb;
    if (amount <= 0) {
        if (out) { out->files_deleted = 0; out->mb_freed = 0; out->new_usage_mb = g_last_scan.total_mb; out->new_usage_pct = (limit_mb > 0) ? (g_last_scan.total_mb / limit_mb * 100.0) : 0; }
        return FIFO_OK;
    }

    auto stats = execute_cleanup(g_db, g_last_scan.all_files, amount);

    if (out) {
        out->files_deleted = stats.files_deleted;
        out->mb_freed = stats.mb_freed;
        out->new_usage_mb = g_last_scan.total_mb - stats.mb_freed;
        out->new_usage_pct = (limit_mb > 0) ? (out->new_usage_mb / limit_mb * 100.0) : 0;
    }
    return FIFO_OK;
}

FIFO_API int fifo_execute_full(const char* root, int granularity, double limit_mb,
                               double target_pct, FullResult* out) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_db.is_open()) return FIFO_ERR_DB;

    // Phase 1: Scan
    g_last_scan = scan_directory(root, granularity);
    store_scan_results(g_db, g_last_scan);

    // Phase 2: Forecast
    g_last_forecast = compute_forecast(g_db, g_last_scan.total_mb);
    store_forecast(g_db, g_last_forecast);

    // Phase 3: Evaluate
    double amount = 0;
    int action = evaluate_threshold(g_last_forecast.predicted_mb, limit_mb, &amount);

    int files_deleted = 0;
    double mb_freed = 0;

    // Phase 4: Cleanup if needed
    if (action == FIFO_ACTION_CLEANUP && amount > 0) {
        auto stats = execute_cleanup(g_db, g_last_scan.all_files, amount);
        files_deleted = stats.files_deleted;
        mb_freed = stats.mb_freed;
    }

    // Record run
    time_t now = time(nullptr);
    struct tm lt;
    localtime_s(&lt, &now);
    char ts[32];
    snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
             lt.tm_hour, lt.tm_min, lt.tm_sec);
    g_db.set_config("last_run", ts);

    if (out) {
        out->current_mb = g_last_scan.total_mb;
        out->predicted_mb = g_last_forecast.predicted_mb;
        out->growth_rate = g_last_forecast.growth_rate;
        out->limit_mb = limit_mb;
        out->usage_pct = (limit_mb > 0) ? (g_last_scan.total_mb / limit_mb * 100.0) : 0;
        out->action = action;
        out->files_deleted = files_deleted;
        out->mb_freed = mb_freed;
        out->history_days = g_last_forecast.days_available;
    }
    return FIFO_OK;
}

FIFO_API int fifo_generate_test_data(const char* root_path, double size_gb, ProgressCallback cb) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_db.is_open()) return FIFO_ERR_DB;
    return generate_test_data(g_db, root_path, size_gb, cb);
}

FIFO_API int fifo_generate_one_day(const char* root_path, double day_size_mb,
                                   int day_offset, ProgressCallback cb) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_db.is_open()) return FIFO_ERR_DB;
    return generate_one_day(g_db, root_path, day_size_mb, day_offset, cb);
}

FIFO_API int fifo_get_weights(WeightInfo* buf, int buf_size, int* out_count) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_db.is_open()) return FIFO_ERR_DB;
    auto weights = g_db.get_average_weights(14);
    int count = (int)weights.size();
    if (count > buf_size) count = buf_size;
    for (int i = 0; i < count; ++i) {
        memset(&buf[i], 0, sizeof(WeightInfo));
        strncpy(buf[i].asset, weights[i].asset.c_str(), 63);
        buf[i].index_val = weights[i].index_val;
        buf[i].category = weights[i].category;
        buf[i].avg_mb = weights[i].avg_mb;
        buf[i].total_mb = weights[i].total_mb;
        buf[i].day_count = weights[i].day_count;
    }
    if (out_count) *out_count = count;
    return FIFO_OK;
}

FIFO_API int fifo_get_history_day_count() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_db.is_open()) return 0;
    return g_db.get_history_day_count();
}

FIFO_API int fifo_schedule_start(const char* root, int granularity,
                                 double limit_mb, double target_pct,
                                 int hour, int minute) {
    if (g_scheduler.is_running()) return FIFO_ERR_BUSY;

    SchedulerConfig cfg;
    cfg.root_path = root;
    cfg.granularity = granularity;
    cfg.limit_mb = limit_mb;
    cfg.target_pct = target_pct;
    cfg.hour = hour;
    cfg.minute = minute;
    cfg.interval_minutes = 0;

    g_scheduler.start(cfg, g_db_path);
    return FIFO_OK;
}

FIFO_API int fifo_schedule_stop() {
    g_scheduler.stop();
    return FIFO_OK;
}

FIFO_API int fifo_schedule_start_interval(const char* root, int granularity,
                                          double limit_mb, double target_pct,
                                          int interval_minutes) {
    if (g_scheduler.is_running()) return FIFO_ERR_BUSY;

    SchedulerConfig cfg;
    cfg.root_path = root;
    cfg.granularity = granularity;
    cfg.limit_mb = limit_mb;
    cfg.target_pct = target_pct;
    cfg.hour = 0;
    cfg.minute = 0;
    cfg.interval_minutes = interval_minutes;

    g_scheduler.start(cfg, g_db_path);
    return FIFO_OK;
}

FIFO_API int fifo_get_status(StatusInfo* out) {
    if (!out) return FIFO_ERR_DB;
    std::lock_guard<std::mutex> lock(g_mutex);

    out->is_scheduled = g_scheduler.is_running() ? 1 : 0;
    out->current_mb = g_last_scan.total_mb;
    out->predicted_mb = g_last_forecast.predicted_mb;
    out->last_action = 0;

    std::string lr = g_db.is_open() ? g_db.get_config("last_run", "") : "";
    strncpy(out->last_run, lr.c_str(), 31);
    out->last_run[31] = 0;

    std::string nr = g_scheduler.next_run();
    strncpy(out->next_run, nr.c_str(), 31);
    out->next_run[31] = 0;

    return FIFO_OK;
}

FIFO_API int fifo_set_config(const char* key, const char* value) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_db.is_open()) return FIFO_ERR_DB;
    return g_db.set_config(key, value);
}

FIFO_API int fifo_get_config(const char* key, char* value_buf, int buf_size) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_db.is_open()) return FIFO_ERR_DB;
    std::string val = g_db.get_config(key, "");
    strncpy(value_buf, val.c_str(), buf_size - 1);
    value_buf[buf_size - 1] = 0;
    return FIFO_OK;
}
