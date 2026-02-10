#include "scanner.h"
#include <algorithm>
#include <map>
#include <ctime>
#include <cstring>
#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static bool is_number(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

static std::string path_join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    char last = a.back();
    if (last == '\\' || last == '/') return a + b;
    return a + "\\" + b;
}

struct DirEntry {
    std::string name;
    bool is_dir;
    ULONGLONG size;
    FILETIME ft_write;
};

static std::vector<DirEntry> list_dir(const std::string& dir) {
    std::vector<DirEntry> entries;
    WIN32_FIND_DATAA fd;
    std::string pattern = path_join(dir, "*");
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return entries;
    do {
        if (fd.cFileName[0] == '.' && (fd.cFileName[1] == 0 ||
            (fd.cFileName[1] == '.' && fd.cFileName[2] == 0))) continue;
        DirEntry e;
        e.name = fd.cFileName;
        e.is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        e.size = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        e.ft_write = fd.ftLastWriteTime;
        entries.push_back(e);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return entries;
}

static time_t filetime_to_time_t(const FILETIME& ft) {
    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    return (time_t)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
}

ScanResult scan_directory(const std::string& root_path, int granularity) {
    ScanResult result{};
    result.total_mb = 0;
    result.total_files = 0;

    struct AggKey {
        std::string asset;
        int index_val;
        char category;
        bool operator<(const AggKey& o) const {
            if (asset != o.asset) return asset < o.asset;
            if (index_val != o.index_val) return index_val < o.index_val;
            return category < o.category;
        }
    };
    std::map<AggKey, ScanEntry> agg;

    // Level 1: ASSET folders
    for (auto& asset_e : list_dir(root_path)) {
        if (!asset_e.is_dir) continue;
        std::string asset_path = path_join(root_path, asset_e.name);

        // Level 2: Index folders
        for (auto& idx_e : list_dir(asset_path)) {
            if (!idx_e.is_dir || !is_number(idx_e.name)) continue;
            int idx_val = std::stoi(idx_e.name);
            std::string idx_path = path_join(asset_path, idx_e.name);

            // Level 3: E or F
            for (auto& cat_e : list_dir(idx_path)) {
                if (!cat_e.is_dir) continue;
                if (cat_e.name != "E" && cat_e.name != "F") continue;
                char cat = cat_e.name[0];
                std::string cat_path = path_join(idx_path, cat_e.name);

                // Level 4: Year
                for (auto& year_e : list_dir(cat_path)) {
                    if (!year_e.is_dir || !is_number(year_e.name) || year_e.name.size() != 4) continue;
                    std::string year_path = path_join(cat_path, year_e.name);

                    // Level 5: Month
                    for (auto& month_e : list_dir(year_path)) {
                        if (!month_e.is_dir || !is_number(month_e.name) || month_e.name.size() != 2) continue;
                        std::string month_path = path_join(year_path, month_e.name);

                        // Level 6: Day
                        for (auto& day_e : list_dir(month_path)) {
                            if (!day_e.is_dir || !is_number(day_e.name) || day_e.name.size() != 2) continue;
                            std::string day_path = path_join(month_path, day_e.name);
                            std::string date = year_e.name + "-" + month_e.name + "-" + day_e.name;

                            // Files in day folder
                            for (auto& file_e : list_dir(day_path)) {
                                if (file_e.is_dir) continue;
                                double size = (double)file_e.size / (1024.0 * 1024.0);

                                ScannedFile sf;
                                sf.full_path = path_join(day_path, file_e.name);
                                sf.size_mb = size;
                                sf.created_time = filetime_to_time_t(file_e.ft_write);
                                sf.asset = asset_e.name;
                                sf.index_val = idx_val;
                                sf.category = cat;
                                sf.date = date;
                                result.all_files.push_back(sf);

                                result.total_mb += size;
                                result.total_files++;

                                AggKey key;
                                key.asset = asset_e.name;
                                key.index_val = (granularity >= 1) ? idx_val : -1;
                                key.category = (granularity >= 2) ? cat : '*';

                                auto& e = agg[key];
                                e.asset = key.asset;
                                e.index_val = key.index_val;
                                e.category = key.category;
                                e.size_mb += size;
                                e.file_count++;
                            }
                        }
                    }
                }
            }
        }
    }

    time_t now = time(nullptr);
    struct tm lt;
    localtime_s(&lt, &now);
    char today[16];
    snprintf(today, sizeof(today), "%04d-%02d-%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);

    for (auto& kv : agg) {
        ScanEntry e = kv.second;
        e.date = today;
        result.entries.push_back(e);
    }

    return result;
}

int store_scan_results(Database& db, const ScanResult& result) {
    for (auto& e : result.entries) {
        StorageRecord rec;
        rec.asset = e.asset;
        rec.index_val = e.index_val;
        rec.category = e.category;
        rec.date = e.date;
        rec.size_mb = e.size_mb;
        rec.file_count = e.file_count;
        if (db.insert_snapshot(rec) != 0) return -1;
    }
    return 0;
}
