using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using FIFOManagement.Models;

namespace FIFOManagement.Services
{
    public class FIFOEngine
    {
        private readonly DatabaseService _dbService;

        public FIFOEngine(DatabaseService dbService)
        {
            _dbService = dbService;
        }

        public int DeleteOldestFiles(string assetId, int count, string reason = "FIFO Cleanup")
        {
            var files = _dbService.GetAllDataFiles()
                .Where(f => f.AssetId == assetId)
                .OrderBy(f => f.CreatedTimestamp)
                .Take(count)
                .ToList();

            int deleted = 0;
            foreach (var file in files)
            {
                try
                {
                    if (File.Exists(file.FilePath))
                    {
                        File.Delete(file.FilePath);
                    }
                    deleted++;
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error deleting {file.FilePath}: {ex.Message}");
                }
            }

            return deleted;
        }

        public double CalculateStorageUsage(string storagePath)
        {
            if (!Directory.Exists(storagePath))
                return 0;

            var dirInfo = new DirectoryInfo(storagePath);
            var files = dirInfo.GetFiles("*", SearchOption.AllDirectories);
            return files.Sum(f => f.Length) / (1024.0 * 1024.0);
        }

        public List<DataFile> GetFilesByAsset(string assetId)
        {
            return _dbService.GetAllDataFiles()
                .Where(f => f.AssetId == assetId)
                .OrderBy(f => f.CreatedTimestamp)
                .ToList();
        }
    }
}
