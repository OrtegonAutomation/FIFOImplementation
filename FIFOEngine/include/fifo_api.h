#ifndef FIFO_API_H
#define FIFO_API_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef FIFO_ENGINE_EXPORTS
    #define FIFO_API __declspec(dllexport)
#else
    #define FIFO_API __declspec(dllimport)
#endif

// Error codes
#define FIFO_OK             0
#define FIFO_ERR_DB        -1
#define FIFO_ERR_PATH      -2
#define FIFO_ERR_SCAN      -3
#define FIFO_ERR_FORECAST  -4
#define FIFO_ERR_CLEANUP   -5
#define FIFO_ERR_BUSY      -6
#define FIFO_ERR_NODATA    -7

// Granularity levels
#define FIFO_GRAN_ASSET         0
#define FIFO_GRAN_ASSET_INDEX   1
#define FIFO_GRAN_ASSET_IDX_CAT 2

// Evaluation actions
#define FIFO_ACTION_SAFE    0
#define FIFO_ACTION_MONITOR 1
#define FIFO_ACTION_CAUTION 2
#define FIFO_ACTION_CLEANUP 3

// Progress callback
typedef void (*ProgressCallback)(int percent, const char* message);

// Result structs (C-compatible, fixed-size)
#pragma pack(push, 8)

typedef struct {
    double current_mb;
    double predicted_mb;
    double growth_rate_mb_per_day;
    int    history_days_available;
} ForecastResult;

typedef struct {
    int    action;           // FIFO_ACTION_*
    double projected_pct;
    double amount_to_delete_mb;
} EvalResult;

typedef struct {
    int    files_deleted;
    double mb_freed;
    double new_usage_mb;
    double new_usage_pct;
} CleanupResult;

typedef struct {
    double current_mb;
    double predicted_mb;
    double growth_rate;
    double limit_mb;
    double usage_pct;
    int    action;
    int    files_deleted;
    double mb_freed;
    int    history_days;
} FullResult;

typedef struct {
    int  is_scheduled;
    int  schedule_hour;
    int  schedule_minute;
    char last_run[32];
    char next_run[32];
    double current_mb;
    double predicted_mb;
    int  last_action;
} StatusInfo;

#pragma pack(pop)

// Core API
FIFO_API int  fifo_init(const char* db_path);
FIFO_API void fifo_shutdown();

// Operations
FIFO_API int fifo_scan(const char* root_path, int granularity);
FIFO_API int fifo_forecast(ForecastResult* out_result);
FIFO_API int fifo_evaluate(double limit_mb, EvalResult* out_result);
FIFO_API int fifo_cleanup(double limit_mb, double target_pct, CleanupResult* out_result);
FIFO_API int fifo_execute_full(const char* root, int granularity, double limit_mb,
                               double target_pct, FullResult* out_result);

// Test data
FIFO_API int fifo_generate_test_data(const char* root_path, double size_gb,
                                     ProgressCallback cb);

// Scheduler
FIFO_API int  fifo_schedule_start(const char* root, int granularity,
                                  double limit_mb, double target_pct,
                                  int hour, int minute);
FIFO_API int  fifo_schedule_stop();
FIFO_API int  fifo_get_status(StatusInfo* out);

// Configuration
FIFO_API int fifo_set_config(const char* key, const char* value);
FIFO_API int fifo_get_config(const char* key, char* value_buf, int buf_size);

#ifdef __cplusplus
}
#endif

#endif // FIFO_API_H
