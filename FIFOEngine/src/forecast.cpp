#include "forecast.h"
#include <algorithm>
#include <numeric>
#include <ctime>
#include <cstdio>
#include <map>

ForecastData compute_forecast(Database& db, double current_total_mb) {
    ForecastData fd{};
    fd.current_mb = current_total_mb;

    // Get last 14 days of history, aggregated by date
    auto history = db.get_history(14);

    // Aggregate per date (sum all entities for each day)
    std::map<std::string, double> daily_totals;
    for (auto& rec : history) {
        daily_totals[rec.date] += rec.size_mb;
    }

    // Build sorted daily values
    std::vector<std::pair<std::string, double>> sorted_days(daily_totals.begin(), daily_totals.end());
    std::sort(sorted_days.begin(), sorted_days.end());

    fd.days_available = (int)sorted_days.size();

    if (fd.days_available == 0) {
        // No history: prediction = current
        fd.predicted_mb = current_total_mb;
        fd.growth_rate = 0;
        return fd;
    }

    if (fd.days_available == 1) {
        // Only one day: no trend
        fd.predicted_mb = current_total_mb;
        fd.growth_rate = 0;
        return fd;
    }

    // Calculate 7-day moving average (use last 7 or whatever is available)
    int window = std::min(7, fd.days_available);
    double sum = 0;
    for (int i = fd.days_available - window; i < fd.days_available; ++i) {
        sum += sorted_days[i].second;
    }
    double moving_avg = sum / window;

    // Calculate linear growth trend
    double first_val = sorted_days.front().second;
    double last_val = sorted_days.back().second;
    fd.growth_rate = (last_val - first_val) / fd.days_available;

    // Forecast tomorrow
    fd.predicted_mb = moving_avg + fd.growth_rate;

    // Don't forecast negative
    if (fd.predicted_mb < 0) fd.predicted_mb = 0;

    return fd;
}

int store_forecast(Database& db, const ForecastData& data) {
    // Forecast date = tomorrow
    time_t now = time(nullptr);
    time_t tomorrow = now + 86400;
    struct tm lt;
    localtime_s(&lt, &tomorrow);
    char date[16];
    snprintf(date, sizeof(date), "%04d-%02d-%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
    return db.insert_forecast(date, data.predicted_mb);
}
