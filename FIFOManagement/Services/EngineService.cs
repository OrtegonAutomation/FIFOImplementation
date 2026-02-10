using System;
using System.IO;
using System.Threading.Tasks;
using FIFOManagement.Interop;

namespace FIFOManagement.Services
{
    public class EngineService : IDisposable
    {
        private bool _initialized;
        private ProgressCallback? _progressDelegate;

        public string DbPath { get; private set; } = string.Empty;

        public bool IsInitialized => _initialized;

        public int Initialize(string dbPath)
        {
            if (_initialized) return FIFOError.OK;
            DbPath = dbPath;
            int rc = FIFONative.fifo_init(dbPath);
            if (rc == FIFOError.OK) _initialized = true;
            return rc;
        }

        public void Shutdown()
        {
            if (!_initialized) return;
            FIFONative.fifo_shutdown();
            _initialized = false;
        }

        public Task<FullResult> ExecuteFullAsync(string rootPath, int granularity, double limitMb, double targetPct = 0.70)
        {
            return Task.Run(() =>
            {
                var result = new FullResult();
                int rc = FIFONative.fifo_execute_full(rootPath, granularity, limitMb, targetPct, ref result);
                if (rc != FIFOError.OK)
                    throw new EngineException($"Execute full failed (code {rc})", rc);
                return result;
            });
        }

        public Task<ForecastResult> ScanAndForecastAsync(string rootPath, int granularity)
        {
            return Task.Run(() =>
            {
                int rc = FIFONative.fifo_scan(rootPath, granularity);
                if (rc != FIFOError.OK && rc != FIFOError.ERR_NODATA)
                    throw new EngineException($"Scan failed (code {rc})", rc);

                var forecast = new ForecastResult();
                rc = FIFONative.fifo_forecast(ref forecast);
                if (rc != FIFOError.OK)
                    throw new EngineException($"Forecast failed (code {rc})", rc);
                return forecast;
            });
        }

        public Task<int> GenerateTestDataAsync(string rootPath, double sizeGb, Action<int, string>? progress = null)
        {
            return Task.Run(() =>
            {
                _progressDelegate = (pct, msg) => progress?.Invoke(pct, msg);
                int rc = FIFONative.fifo_generate_test_data(rootPath, sizeGb, _progressDelegate);
                _progressDelegate = null;
                return rc;
            });
        }

        public Task<int> GenerateOneDayAsync(string rootPath, double daySizeMb, int dayOffset, Action<int, string>? progress = null)
        {
            return Task.Run(() =>
            {
                _progressDelegate = (pct, msg) => progress?.Invoke(pct, msg);
                int rc = FIFONative.fifo_generate_one_day(rootPath, daySizeMb, dayOffset, _progressDelegate);
                _progressDelegate = null;
                return rc;
            });
        }

        public Task<CleanupResult> ForceCleanupAsync(string rootPath, int granularity, double limitMb, double targetPct)
        {
            return Task.Run(() =>
            {
                int rc = FIFONative.fifo_scan(rootPath, granularity);
                if (rc != FIFOError.OK && rc != FIFOError.ERR_NODATA)
                    throw new EngineException($"Scan failed (code {rc})", rc);

                var result = new CleanupResult();
                rc = FIFONative.fifo_cleanup(limitMb, targetPct, ref result);
                if (rc != FIFOError.OK)
                    throw new EngineException($"Cleanup failed (code {rc})", rc);
                return result;
            });
        }

        public WeightInfo[] GetWeights()
        {
            var buf = new WeightInfo[200];
            int count;
            int rc = FIFONative.fifo_get_weights(buf, buf.Length, out count);
            if (rc != FIFOError.OK) return Array.Empty<WeightInfo>();
            var result = new WeightInfo[count];
            Array.Copy(buf, result, count);
            return result;
        }

        public int GetHistoryDayCount()
        {
            return FIFONative.fifo_get_history_day_count();
        }

        public int StartSchedule(string rootPath, int granularity, double limitMb, double targetPct, int hour, int minute)
        {
            return FIFONative.fifo_schedule_start(rootPath, granularity, limitMb, targetPct, hour, minute);
        }

        public int StartScheduleInterval(string rootPath, int granularity, double limitMb, double targetPct, int intervalMinutes)
        {
            return FIFONative.fifo_schedule_start_interval(rootPath, granularity, limitMb, targetPct, intervalMinutes);
        }

        public int StopSchedule()
        {
            return FIFONative.fifo_schedule_stop();
        }

        public StatusInfo GetStatus()
        {
            var info = new StatusInfo();
            FIFONative.fifo_get_status(ref info);
            return info;
        }

        public void SetConfig(string key, string value)
        {
            FIFONative.fifo_set_config(key, value);
        }

        public string GetConfig(string key, string defaultValue = "")
        {
            var sb = new System.Text.StringBuilder(256);
            int rc = FIFONative.fifo_get_config(key, sb, sb.Capacity);
            if (rc != FIFOError.OK) return defaultValue;
            string val = sb.ToString();
            return string.IsNullOrEmpty(val) ? defaultValue : val;
        }

        public void Dispose()
        {
            Shutdown();
        }
    }

    public class EngineException : Exception
    {
        public int ErrorCode { get; }
        public EngineException(string message, int errorCode) : base(message)
        {
            ErrorCode = errorCode;
        }
    }
}
