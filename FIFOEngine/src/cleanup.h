#ifndef CLEANUP_H
#define CLEANUP_H

#include "database.h"
#include "scanner.h"
#include <vector>

struct CleanupStats {
    int    files_deleted;
    double mb_freed;
    double new_usage_mb;
};

// Evaluate whether cleanup is needed
// Returns: FIFO_ACTION_SAFE, _MONITOR, _CAUTION, or _CLEANUP
int evaluate_threshold(double predicted_mb, double limit_mb, double* amount_to_delete);

// Execute FIFO cleanup: delete oldest files from E/F until target reached
// min_retention_hours: skip files younger than this (default 24)
// max_deletions: safety limit per cycle (default 500)
CleanupStats execute_cleanup(Database& db, std::vector<ScannedFile>& files,
                             double amount_to_delete_mb,
                             int min_retention_hours = 24,
                             int max_deletions = 500);

#endif // CLEANUP_H
