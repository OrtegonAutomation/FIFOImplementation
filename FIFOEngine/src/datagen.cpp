#include "datagen.h"
#include <algorithm>
#include <fstream>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

static void create_dirs_recursive(const std::string& path) {
    // Create all directories in path
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '\\' || path[i] == '/') {
            std::string sub = path.substr(0, i);
            if (!sub.empty()) CreateDirectoryA(sub.c_str(), NULL);
        }
    }
    CreateDirectoryA(path.c_str(), NULL);
}

static std::string path_join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    char last = a.back();
    if (last == '\\' || last == '/') return a + b;
    return a + "\\" + b;
}

static void write_random_file(const std::string& path, size_t bytes) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return;
    const int BUF_SIZE = 8192;
    char buf[8192];
    size_t written = 0;
    while (written < bytes) {
        size_t chunk = std::min((size_t)BUF_SIZE, bytes - written);
        for (size_t i = 0; i < chunk; ++i)
            buf[i] = (char)(rand() % 256);
        out.write(buf, chunk);
        written += chunk;
    }
}

int generate_test_data(Database& db, const std::string& root_path,
                       double size_gb, ProgressCallback cb) {
    const char* assets[] = { "ASSET_01", "ASSET_02", "ASSET_03" };
    const int num_assets = 3;
    const int num_indices = 5;
    const char cats[] = { 'E', 'F' };
    const int num_cats = 2;
    const int num_days = 14;

    // Total combinations: 3 * 5 * 2 * 14 = 420 day-folders
    // Each folder gets 1 file
    int total_folders = num_assets * num_indices * num_cats * num_days;
    size_t total_bytes = (size_t)(size_gb * 1024.0 * 1024.0 * 1024.0);
    size_t bytes_per_file = total_bytes / total_folders;
    if (bytes_per_file < 1024) bytes_per_file = 1024;

    // Growth factor: day 1 gets 70% of avg, day 14 gets 130% of avg
    int folder_idx = 0;

    time_t now = time(nullptr);

    for (int a = 0; a < num_assets; ++a) {
        for (int idx = 1; idx <= num_indices; ++idx) {
            for (int c = 0; c < num_cats; ++c) {
                double day_total_mb = 0;
                int day_total_count = 0;

                for (int d = 0; d < num_days; ++d) {
                    // Calculate date for this day (d=0 is 13 days ago, d=13 is today)
                    time_t day_time = now - (time_t)(num_days - 1 - d) * 86400;
                    struct tm lt;
                    localtime_s(&lt, &day_time);
                    char year[8], month[4], day_str[4], date[16];
                    snprintf(year, sizeof(year), "%04d", lt.tm_year + 1900);
                    snprintf(month, sizeof(month), "%02d", lt.tm_mon + 1);
                    snprintf(day_str, sizeof(day_str), "%02d", lt.tm_mday);
                    snprintf(date, sizeof(date), "%s-%s-%s", year, month, day_str);

                    // Growth factor: linear ramp
                    double growth = 0.7 + (0.6 * d / (num_days - 1));
                    size_t file_bytes = (size_t)(bytes_per_file * growth);

                    // Build path
                    char cat_str[2] = { cats[c], 0 };
                    std::string dir_path = path_join(
                        path_join(path_join(path_join(path_join(
                            root_path, assets[a]),
                            std::to_string(idx)),
                            cat_str),
                            year),
                            month);
                    dir_path = path_join(dir_path, day_str);

                    create_dirs_recursive(dir_path);

                    std::string file_name = std::string(assets[a]) + "_" +
                                            std::to_string(idx) + "_" +
                                            cat_str + "_" + date + ".dat";
                    std::string file_path = path_join(dir_path, file_name);
                    write_random_file(file_path, file_bytes);

                    double file_mb = (double)file_bytes / (1024.0 * 1024.0);
                    day_total_mb += file_mb;
                    day_total_count++;

                    folder_idx++;
                    if (cb) {
                        int pct = (int)((folder_idx * 100) / total_folders);
                        char msg[128];
                        snprintf(msg, sizeof(msg), "Generating %s/%d/%c day %d/%d",
                                 assets[a], idx, cats[c], d + 1, num_days);
                        cb(pct, msg);
                    }
                }

                // Store aggregated snapshot per day for this entity
                // Re-scan days to store per-day snapshots
                for (int d = 0; d < num_days; ++d) {
                    time_t day_time = now - (time_t)(num_days - 1 - d) * 86400;
                    struct tm lt;
                    localtime_s(&lt, &day_time);
                    char date[16];
                    snprintf(date, sizeof(date), "%04d-%02d-%02d",
                             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);

                    double growth = 0.7 + (0.6 * d / (num_days - 1));
                    double file_mb = ((double)bytes_per_file * growth) / (1024.0 * 1024.0);

                    StorageRecord rec;
                    rec.asset = assets[a];
                    rec.index_val = idx;
                    rec.category = cats[c];
                    rec.date = date;
                    rec.size_mb = file_mb;
                    rec.file_count = 1;
                    db.insert_snapshot(rec);
                }
            }
        }
    }

    if (cb) cb(100, "Test data generation complete");
    return 0;
}

int generate_one_day(Database& db, const std::string& root_path,
                     double day_size_mb, int day_offset, ProgressCallback cb) {
    const char* assets[] = { "ASSET_01", "ASSET_02", "ASSET_03" };
    const int num_assets = 3;
    const int num_indices = 5;
    const char cats[] = { 'E', 'F' };
    const int num_cats = 2;

    int total_entities = num_assets * num_indices * num_cats; // 30
    size_t total_bytes = (size_t)(day_size_mb * 1024.0 * 1024.0);
    size_t bytes_per_file = total_bytes / total_entities;
    if (bytes_per_file < 1024) bytes_per_file = 1024;

    time_t now = time(nullptr);
    time_t day_time = now + (time_t)day_offset * 86400;
    struct tm lt;
    localtime_s(&lt, &day_time);
    char year[8], month[4], day_str[4], date[16];
    snprintf(year, sizeof(year), "%04d", lt.tm_year + 1900);
    snprintf(month, sizeof(month), "%02d", lt.tm_mon + 1);
    snprintf(day_str, sizeof(day_str), "%02d", lt.tm_mday);
    snprintf(date, sizeof(date), "%s-%s-%s", year, month, day_str);

    int entity_idx = 0;
    for (int a = 0; a < num_assets; ++a) {
        for (int idx = 1; idx <= num_indices; ++idx) {
            for (int c = 0; c < num_cats; ++c) {
                // Add some random variation (+/- 20%)
                double variation = 0.8 + (rand() % 40) / 100.0;
                size_t file_bytes = (size_t)(bytes_per_file * variation);

                char cat_str[2] = { cats[c], 0 };
                std::string dir_path = path_join(
                    path_join(path_join(path_join(path_join(
                        root_path, assets[a]),
                        std::to_string(idx)),
                        cat_str),
                        year),
                        month);
                dir_path = path_join(dir_path, day_str);

                create_dirs_recursive(dir_path);

                std::string file_name = std::string(assets[a]) + "_" +
                                        std::to_string(idx) + "_" +
                                        cat_str + "_" + date + ".dat";
                std::string file_path = path_join(dir_path, file_name);
                write_random_file(file_path, file_bytes);

                double file_mb = (double)file_bytes / (1024.0 * 1024.0);

                // Store in DB
                StorageRecord rec;
                rec.asset = assets[a];
                rec.index_val = idx;
                rec.category = cats[c];
                rec.date = date;
                rec.size_mb = file_mb;
                rec.file_count = 1;
                db.insert_snapshot(rec);

                entity_idx++;
                if (cb) {
                    int pct = (int)((entity_idx * 100) / total_entities);
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Day %s: %s/%d/%c", date, assets[a], idx, cats[c]);
                    cb(pct, msg);
                }
            }
        }
    }

    if (cb) cb(100, "One day of data generated");
    return 0;
}
