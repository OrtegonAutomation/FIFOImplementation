using System;

namespace FIFOManagement.Models
{
    public class StoragePath
    {
        public int PathId { get; set; }
        public string StoragePathValue { get; set; }
        public double TotalCapacityGB { get; set; }
        public double WarningThreshold { get; set; }
        public double CriticalThreshold { get; set; }
        public bool IsMonitored { get; set; }
        public DateTime CreatedAt { get; set; }
    }
}
