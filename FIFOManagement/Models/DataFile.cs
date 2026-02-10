using System;

namespace FIFOManagement.Models
{
    public class DataFile
    {
        public int FileId { get; set; }
        public string AssetId { get; set; }
        public int StoragePathId { get; set; }
        public string FilePath { get; set; }
        public double FileSizeMB { get; set; }
        public DateTime CreatedTimestamp { get; set; }
        public DateTime IngestedTimestamp { get; set; }
        public string Status { get; set; }
        public DateTime? DeletedTimestamp { get; set; }
    }
}
