using System;
using System.IO;
using System.Threading.Tasks;

namespace FIFOManagement.Services
{
    public class DataGenerator
    {
        public async Task<string> GenerateTestFile(string directory, string assetId, double sizeMB)
        {
            if (!Directory.Exists(directory))
                Directory.CreateDirectory(directory);

            string timestamp = DateTime.Now.ToString("yyyyMMdd_HHmmss");
            string fileName = $"{assetId}_{timestamp}.dat";
            string filePath = Path.Combine(directory, fileName);

            int bytesToWrite = (int)(sizeMB * 1024 * 1024);
            byte[] buffer = new byte[8192];
            Random random = new Random();

            using (FileStream fs = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.None, 8192, true))
            {
                int written = 0;
                while (written < bytesToWrite)
                {
                    int chunkSize = Math.Min(buffer.Length, bytesToWrite - written);
                    random.NextBytes(buffer);
                    await fs.WriteAsync(buffer, 0, chunkSize);
                    written += chunkSize;
                }
            }

            return filePath;
        }

        public async Task GenerateMultipleFiles(string directory, string assetId, int count, double sizeMB)
        {
            for (int i = 0; i < count; i++)
            {
                await GenerateTestFile(directory, assetId, sizeMB);
                await Task.Delay(100);
            }
        }
    }
}
