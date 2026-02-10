#include "cleanup.h"
#include "fifo_api.h"
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

int evaluate_threshold(double predicted_mb, double limit_mb, double* amount_to_delete) {
    if (limit_mb <= 0) {
        if (amount_to_delete) *amount_to_delete = 0;
        return FIFO_ACTION_SAFE;
    }

    double pct = (predicted_mb / limit_mb) * 100.0;

    if (pct < 85.0) {
        if (amount_to_delete) *amount_to_delete = 0;
        return FIFO_ACTION_SAFE;
    }
    if (pct < 90.0) {
        if (amount_to_delete) *amount_to_delete = 0;
        return FIFO_ACTION_MONITOR;
    }
    if (pct < 95.0) {
        if (amount_to_delete) *amount_to_delete = 0;
        return FIFO_ACTION_CAUTION;
    }

    // Cleanup needed: reduce to 70% target
    double target_mb = limit_mb * 0.70;
    double to_delete = predicted_mb - target_mb;
    if (to_delete < 0) to_delete = 0;
    if (amount_to_delete) *amount_to_delete = to_delete;
    return FIFO_ACTION_CLEANUP;
}

CleanupStats execute_cleanup(Database& db, std::vector<ScannedFile>& files,
                             double amount_to_delete_mb,
                             int min_retention_hours, int max_deletions) {
    CleanupStats stats{};
    if (amount_to_delete_mb <= 0 || files.empty()) return stats;

    time_t now = time(nullptr);
    time_t cutoff = now - (min_retention_hours * 3600);

    // Sort files by creation time ascending (oldest first)
    std::sort(files.begin(), files.end(), [](const ScannedFile& a, const ScannedFile& b) {
        return a.created_time < b.created_time;
    });

    // Count files per asset-index-category to avoid deleting everything
    struct EntityKey {
        std::string asset;
        int index_val;
        char category;
        bool operator<(const EntityKey& o) const {
            if (asset != o.asset) return asset < o.asset;
            if (index_val != o.index_val) return index_val < o.index_val;
            return category < o.category;
        }
    };
    std::map<EntityKey, int> entity_counts;
    for (auto& f : files) {
        entity_counts[{f.asset, f.index_val, f.category}]++;
    }

    double freed = 0;
    int count = 0;

    for (auto& f : files) {
        if (freed >= amount_to_delete_mb || count >= max_deletions)
            break;

        // Skip files newer than retention period
        if (f.created_time > cutoff)
            continue;

        // Keep minimum 5 files per entity
        EntityKey ek = {f.asset, f.index_val, f.category};
        if (entity_counts[ek] <= 5)
            continue;

        // Delete file
        try {
            if (DeleteFileA(f.full_path.c_str())) {
                // Log deletion
                DeletionRecord dr;
                dr.file_path = f.full_path;
                dr.asset = f.asset;
                dr.size_mb = f.size_mb;
                dr.reason = "PREDICTIVE_CLEANUP";
                db.log_deletion(dr);

                freed += f.size_mb;
                count++;
                entity_counts[ek]--;
            }
        } catch (...) {
            // Skip files we can't delete
        }
    }

    stats.files_deleted = count;
    stats.mb_freed = freed;
    return stats;
}
