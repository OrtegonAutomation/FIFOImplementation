using System;

namespace FIFOManagement.Models
{
    public class Asset
    {
        public string AssetId { get; set; }
        public string AssetName { get; set; }
        public double AverageWeightMB { get; set; }
        public int MinimumRetentionHours { get; set; }
        public int MaximumRetentionDays { get; set; }
        public string Priority { get; set; }
        public DateTime CreatedAt { get; set; }
    }
}
