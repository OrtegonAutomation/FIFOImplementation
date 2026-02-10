#ifndef DATAGEN_H
#define DATAGEN_H

#include "database.h"
#include "fifo_api.h"
#include <string>

// Generate 14 days of synthetic test data
// Creates: 3 assets x 5 indices x 2 categories (E/F) x 14 days
// Total size approximately size_gb
int generate_test_data(Database& db, const std::string& root_path,
                       double size_gb, ProgressCallback cb);

// Generate one day of synthetic test data (appends to existing data)
// day_offset: 0=today, 1=tomorrow, -1=yesterday, etc.
// day_size_mb: total MB to generate for this day spread across all entities
int generate_one_day(Database& db, const std::string& root_path,
                     double day_size_mb, int day_offset, ProgressCallback cb);

#endif // DATAGEN_H
