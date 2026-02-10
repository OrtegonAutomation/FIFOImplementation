using System;

namespace FIFOManagement.Models
{
    public class DeletionLog
    {
        public int LogId { get; set; }
        public int FileId { get; set; }
        public string AssetId { get; set; }
        public string FilePath { get; set; }
        public string DeletionReason { get; set; }
        public double SpaceFreedMB { get; set; }
        public DateTime DeletionTimestamp { get; set; }
        public string TriggeredBy { get; set; }
    }
}
