#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <string>
#include <atomic>
#include <thread>
#include <functional>

struct SchedulerConfig {
    std::string root_path;
    int granularity;
    double limit_mb;
    double target_pct;
    int hour;
    int minute;
    int interval_minutes;  // 0 = daily mode, >0 = interval mode
};

class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    void start(const SchedulerConfig& config, const std::string& db_path);
    void stop();
    bool is_running() const { return running_.load(); }

    // Execute one full cycle immediately
    static int execute_once(const std::string& db_path, const SchedulerConfig& config);

    std::string last_run() const { return last_run_; }
    std::string next_run() const;

private:
    void run_loop();

    std::atomic<bool> running_{false};
    std::thread thread_;
    SchedulerConfig config_;
    std::string db_path_;
    std::string last_run_;
};

#endif // SCHEDULER_H
