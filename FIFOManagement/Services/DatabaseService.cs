using System;
using System.Collections.Generic;
using System.Data.SQLite;
using System.IO;
using FIFOManagement.Models;

namespace FIFOManagement.Services
{
    public class DatabaseService
    {
        private readonly string _connectionString;

        public DatabaseService(string dbPath)
        {
            _connectionString = $"Data Source={dbPath};Version=3;";
        }

        public void InitializeDatabase()
        {
            using var connection = new SQLiteConnection(_connectionString);
            connection.Open();

            string[] createTableCommands = new[]
            {
                @"CREATE TABLE IF NOT EXISTS assets (
                    asset_id TEXT PRIMARY KEY,
                    asset_name TEXT NOT NULL,
                    average_weight_mb REAL NOT NULL DEFAULT 50.0,
                    minimum_retention_hours INTEGER NOT NULL DEFAULT 24,
                    maximum_retention_days INTEGER NOT NULL DEFAULT 30,
                    priority TEXT NOT NULL DEFAULT 'NORMAL',
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )",
                @"CREATE TABLE IF NOT EXISTS storage_paths (
                    path_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    storage_path TEXT NOT NULL UNIQUE,
                    total_capacity_gb REAL NOT NULL,
                    warning_threshold REAL NOT NULL DEFAULT 0.75,
                    critical_threshold REAL NOT NULL DEFAULT 0.90,
                    is_monitored BOOLEAN NOT NULL DEFAULT 1,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )",
                @"CREATE TABLE IF NOT EXISTS data_files (
                    file_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    asset_id TEXT NOT NULL,
                    storage_path_id INTEGER NOT NULL,
                    file_path TEXT NOT NULL UNIQUE,
                    file_size_mb REAL NOT NULL,
                    created_timestamp TIMESTAMP NOT NULL,
                    ingested_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    status TEXT NOT NULL DEFAULT 'ACTIVE',
                    deleted_timestamp TIMESTAMP,
                    FOREIGN KEY (asset_id) REFERENCES assets(asset_id),
                    FOREIGN KEY (storage_path_id) REFERENCES storage_paths(path_id)
                )",
                @"CREATE TABLE IF NOT EXISTS deletion_log (
                    log_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    file_id INTEGER NOT NULL,
                    asset_id TEXT NOT NULL,
                    file_path TEXT NOT NULL,
                    deletion_reason TEXT NOT NULL,
                    space_freed_mb REAL NOT NULL,
                    deletion_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    triggered_by TEXT NOT NULL,
                    FOREIGN KEY (file_id) REFERENCES data_files(file_id),
                    FOREIGN KEY (asset_id) REFERENCES assets(asset_id)
                )",
                @"CREATE INDEX IF NOT EXISTS idx_fifo_queue 
                  ON data_files(asset_id, created_timestamp, status)",
                @"CREATE INDEX IF NOT EXISTS idx_storage_path 
                  ON data_files(storage_path_id, status)",
                @"CREATE INDEX IF NOT EXISTS idx_deletion_timestamp 
                  ON deletion_log(deletion_timestamp)"
            };

            using var command = new SQLiteCommand(connection);
            foreach (var sql in createTableCommands)
            {
                command.CommandText = sql;
                command.ExecuteNonQuery();
            }
        }

        public void InsertSampleData()
        {
            using var connection = new SQLiteConnection(_connectionString);
            connection.Open();

            using var command = new SQLiteCommand(connection);
            
            command.CommandText = @"INSERT OR IGNORE INTO assets 
                (asset_id, asset_name, average_weight_mb, minimum_retention_hours, maximum_retention_days, priority)
                VALUES (@id, @name, @weight, @minHours, @maxDays, @priority)";

            var sampleAssets = new[]
            {
                new { id = "ASSET_001", name = "Sensor de Temperatura - Línea 1", weight = 50.0, minHours = 2, maxDays = 7, priority = "HIGH" },
                new { id = "ASSET_002", name = "Monitor de Presión - Línea 2", weight = 75.0, minHours = 4, maxDays = 14, priority = "NORMAL" },
                new { id = "ASSET_003", name = "Controlador PLC - Línea 3", weight = 30.0, minHours = 1, maxDays = 7, priority = "HIGH" }
            };

            foreach (var asset in sampleAssets)
            {
                command.Parameters.Clear();
                command.Parameters.AddWithValue("@id", asset.id);
                command.Parameters.AddWithValue("@name", asset.name);
                command.Parameters.AddWithValue("@weight", asset.weight);
                command.Parameters.AddWithValue("@minHours", asset.minHours);
                command.Parameters.AddWithValue("@maxDays", asset.maxDays);
                command.Parameters.AddWithValue("@priority", asset.priority);
                command.ExecuteNonQuery();
            }
        }

        public List<Asset> GetAllAssets()
        {
            var assets = new List<Asset>();
            using var connection = new SQLiteConnection(_connectionString);
            connection.Open();

            using var command = new SQLiteCommand("SELECT * FROM assets", connection);
            using var reader = command.ExecuteReader();

            while (reader.Read())
            {
                assets.Add(new Asset
                {
                    AssetId = reader.GetString(0),
                    AssetName = reader.GetString(1),
                    AverageWeightMB = reader.GetDouble(2),
                    MinimumRetentionHours = reader.GetInt32(3),
                    MaximumRetentionDays = reader.GetInt32(4),
                    Priority = reader.GetString(5),
                    CreatedAt = reader.GetDateTime(6)
                });
            }

            return assets;
        }

        public List<DataFile> GetAllDataFiles()
        {
            var files = new List<DataFile>();
            using var connection = new SQLiteConnection(_connectionString);
            connection.Open();

            using var command = new SQLiteCommand("SELECT * FROM data_files WHERE status = 'ACTIVE' ORDER BY created_timestamp", connection);
            using var reader = command.ExecuteReader();

            while (reader.Read())
            {
                files.Add(new DataFile
                {
                    FileId = reader.GetInt32(0),
                    AssetId = reader.GetString(1),
                    StoragePathId = reader.GetInt32(2),
                    FilePath = reader.GetString(3),
                    FileSizeMB = reader.GetDouble(4),
                    CreatedTimestamp = reader.GetDateTime(5),
                    IngestedTimestamp = reader.GetDateTime(6),
                    Status = reader.GetString(7)
                });
            }

            return files;
        }

        public List<DeletionLog> GetDeletionLogs(int limit = 100)
        {
            var logs = new List<DeletionLog>();
            using var connection = new SQLiteConnection(_connectionString);
            connection.Open();

            using var command = new SQLiteCommand($"SELECT * FROM deletion_log ORDER BY deletion_timestamp DESC LIMIT {limit}", connection);
            using var reader = command.ExecuteReader();

            while (reader.Read())
            {
                logs.Add(new DeletionLog
                {
                    LogId = reader.GetInt32(0),
                    FileId = reader.GetInt32(1),
                    AssetId = reader.GetString(2),
                    FilePath = reader.GetString(3),
                    DeletionReason = reader.GetString(4),
                    SpaceFreedMB = reader.GetDouble(5),
                    DeletionTimestamp = reader.GetDateTime(6),
                    TriggeredBy = reader.GetString(7)
                });
            }

            return logs;
        }

        public void AddStoragePath(string path, double capacityGB)
        {
            using var connection = new SQLiteConnection(_connectionString);
            connection.Open();

            using var command = new SQLiteCommand(@"INSERT OR IGNORE INTO storage_paths 
                (storage_path, total_capacity_gb) VALUES (@path, @capacity)", connection);
            command.Parameters.AddWithValue("@path", path);
            command.Parameters.AddWithValue("@capacity", capacityGB);
            command.ExecuteNonQuery();
        }

        public int RegisterFile(string assetId, int storagePathId, string filePath, double sizeMB, DateTime createdTime)
        {
            using var connection = new SQLiteConnection(_connectionString);
            connection.Open();

            using var command = new SQLiteCommand(@"INSERT INTO data_files 
                (asset_id, storage_path_id, file_path, file_size_mb, created_timestamp)
                VALUES (@assetId, @pathId, @filePath, @size, @created)", connection);
            
            command.Parameters.AddWithValue("@assetId", assetId);
            command.Parameters.AddWithValue("@pathId", storagePathId);
            command.Parameters.AddWithValue("@filePath", filePath);
            command.Parameters.AddWithValue("@size", sizeMB);
            command.Parameters.AddWithValue("@created", createdTime);
            command.ExecuteNonQuery();

            return (int)connection.LastInsertRowId;
        }
    }
}
