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

#endif // DATAGEN_H
