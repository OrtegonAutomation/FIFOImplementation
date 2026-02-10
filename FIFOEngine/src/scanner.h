#ifndef SCANNER_H
#define SCANNER_H

#include "database.h"
#include <string>
#include <vector>

struct ScanEntry {
    std::string asset;
    int         index_val;
    char        category; // 'E' or 'F'
    std::string date;     // YYYY-MM-DD
    double      size_mb;
    int         file_count;
};

struct ScannedFile {
    std::string full_path;
    double      size_mb;
    time_t      created_time;
    std::string asset;
    int         index_val;
    char        category;
    std::string date;
};

// Scan result aggregated by granularity
struct ScanResult {
    double total_mb;
    int    total_files;
    std::vector<ScanEntry> entries;
    std::vector<ScannedFile> all_files;  // needed for cleanup
};

// Scan root following ASSET\Index\E|F\Year\Month\Day schema
ScanResult scan_directory(const std::string& root_path, int granularity);

// Store scan results as today's snapshot in database
int store_scan_results(Database& db, const ScanResult& result);

#endif // SCANNER_H
