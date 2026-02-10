#include "scheduler.h"
#include "database.h"
#include "scanner.h"
#include "forecast.h"
#include "cleanup.h"
#include "fifo_api.h"
#include <chrono>
#include <ctime>
#include <cstdio>

Scheduler::Scheduler() {}

Scheduler::~Scheduler() { stop(); }

void Scheduler::start(const SchedulerConfig& config, const std::string& db_path) {
    if (running_.load()) return;
    config_ = config;
    db_path_ = db_path;
    running_.store(true);
    thread_ = std::thread(&Scheduler::run_loop, this);
}

void Scheduler::stop() {
    running_.store(false);
    if (thread_.joinable()) thread_.join();
}

std::string Scheduler::next_run() const {
    if (!running_.load()) return "";
    time_t now = time(nullptr);
    struct tm lt;
    localtime_s(&lt, &now);

    time_t next_t;
    if (config_.interval_minutes > 0) {
        next_t = now + config_.interval_minutes * 60;
    } else {
        struct tm next = lt;
        next.tm_hour = config_.hour;
        next.tm_min = config_.minute;
        next.tm_sec = 0;
        next_t = mktime(&next);
        if (next_t <= now) next_t += 86400;
    }

    struct tm nr;
    localtime_s(&nr, &next_t);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
             nr.tm_year + 1900, nr.tm_mon + 1, nr.tm_mday,
             nr.tm_hour, nr.tm_min);
    return buf;
}

int Scheduler::execute_once(const std::string& db_path, const SchedulerConfig& config) {
    Database db;
    if (db.open(db_path) != 0) return FIFO_ERR_DB;

    // Phase 1: Scan
    auto scan = scan_directory(config.root_path, config.granularity);
    if (scan.total_files == 0) {
        db.close();
        return FIFO_ERR_NODATA;
    }
    store_scan_results(db, scan);

    // Phase 2: Forecast
    auto forecast = compute_forecast(db, scan.total_mb);
    store_forecast(db, forecast);

    // Phase 3: Evaluate
    double amount = 0;
    int action = evaluate_threshold(forecast.predicted_mb, config.limit_mb, &amount);

    // Phase 4: Cleanup if needed
    if (action == FIFO_ACTION_CLEANUP && amount > 0) {
        execute_cleanup(db, scan.all_files, amount);
    }

    // Store last run time
    time_t now = time(nullptr);
    struct tm lt;
    localtime_s(&lt, &now);
    char ts[32];
    snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
             lt.tm_hour, lt.tm_min, lt.tm_sec);
    db.set_config("last_run", ts);

    db.close();
    return FIFO_OK;
}

void Scheduler::run_loop() {
    while (running_.load()) {
        long wait_secs;
        if (config_.interval_minutes > 0) {
            wait_secs = config_.interval_minutes * 60;
        } else {
            time_t now = time(nullptr);
            struct tm lt;
            localtime_s(&lt, &now);
            struct tm target = lt;
            target.tm_hour = config_.hour;
            target.tm_min = config_.minute;
            target.tm_sec = 0;
            time_t target_t = mktime(&target);
            if (target_t <= now) target_t += 86400;
            wait_secs = (long)(target_t - now);
        }

        // Sleep in 1-second increments to allow stop()
        for (long i = 0; i < wait_secs && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (!running_.load()) break;

        // Execute pipeline
        execute_once(db_path_, config_);

        // Record last run
        time_t now = time(nullptr);
        struct tm lt;
        localtime_s(&lt, &now);
        char ts[32];
        snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
                 lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                 lt.tm_hour, lt.tm_min, lt.tm_sec);
        last_run_ = ts;
    }
}
