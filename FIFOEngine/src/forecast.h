#ifndef FORECAST_H
#define FORECAST_H

#include "database.h"
#include <vector>

struct ForecastData {
    double current_mb;
    double predicted_mb;
    double growth_rate;     // MB per day
    int    days_available;
};

// Calculate moving average forecast for tomorrow
ForecastData compute_forecast(Database& db, double current_total_mb);

// Store forecast result in database
int store_forecast(Database& db, const ForecastData& data);

#endif // FORECAST_H
