using System;
using System.Runtime.InteropServices;

namespace FIFOManagement.Interop
{
    // Error codes
    public static class FIFOError
    {
        public const int OK = 0;
        public const int ERR_DB = -1;
        public const int ERR_PATH = -2;
        public const int ERR_SCAN = -3;
        public const int ERR_FORECAST = -4;
        public const int ERR_CLEANUP = -5;
        public const int ERR_BUSY = -6;
        public const int ERR_NODATA = -7;
    }

    // Granularity levels
    public static class FIFOGranularity
    {
        public const int Asset = 0;
        public const int AssetIndex = 1;
        public const int AssetIndexCategory = 2;
    }

    // Evaluation actions
    public static class FIFOAction
    {
        public const int Safe = 0;
        public const int Monitor = 1;
        public const int Caution = 2;
        public const int Cleanup = 3;

        public static string ToLabel(int action) => action switch
        {
            Safe => "SAFE",
            Monitor => "MONITOR",
            Caution => "CAUTION",
            Cleanup => "CLEANUP",
            _ => "UNKNOWN"
        };
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    public struct ForecastResult
    {
        public double CurrentMB;
        public double PredictedMB;
        public double GrowthRateMBPerDay;
        public int HistoryDaysAvailable;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    public struct EvalResult
    {
        public int Action;
        public double ProjectedPct;
        public double AmountToDeleteMB;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    public struct CleanupResult
    {
        public int FilesDeleted;
        public double MBFreed;
        public double NewUsageMB;
        public double NewUsagePct;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    public struct FullResult
    {
        public double CurrentMB;
        public double PredictedMB;
        public double GrowthRate;
        public double LimitMB;
        public double UsagePct;
        public int Action;
        public int FilesDeleted;
        public double MBFreed;
        public int HistoryDays;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8, CharSet = CharSet.Ansi)]
    public struct StatusInfo
    {
        public int IsScheduled;
        public int ScheduleHour;
        public int ScheduleMinute;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string LastRun;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string NextRun;
        public double CurrentMB;
        public double PredictedMB;
        public int LastAction;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ProgressCallback(int percent, [MarshalAs(UnmanagedType.LPStr)] string message);

    public static class FIFONative
    {
        private const string DllName = "fifo_engine.dll";

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int fifo_init([MarshalAs(UnmanagedType.LPStr)] string dbPath);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void fifo_shutdown();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int fifo_scan([MarshalAs(UnmanagedType.LPStr)] string rootPath, int granularity);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int fifo_forecast(ref ForecastResult outResult);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int fifo_evaluate(double limitMb, ref EvalResult outResult);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int fifo_cleanup(double limitMb, double targetPct, ref CleanupResult outResult);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int fifo_execute_full(
            [MarshalAs(UnmanagedType.LPStr)] string root,
            int granularity, double limitMb, double targetPct,
            ref FullResult outResult);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int fifo_generate_test_data(
            [MarshalAs(UnmanagedType.LPStr)] string rootPath,
            double sizeGb, ProgressCallback cb);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int fifo_schedule_start(
            [MarshalAs(UnmanagedType.LPStr)] string root,
            int granularity, double limitMb, double targetPct,
            int hour, int minute);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int fifo_schedule_stop();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int fifo_get_status(ref StatusInfo outInfo);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int fifo_set_config(
            [MarshalAs(UnmanagedType.LPStr)] string key,
            [MarshalAs(UnmanagedType.LPStr)] string value);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int fifo_get_config(
            [MarshalAs(UnmanagedType.LPStr)] string key,
            [MarshalAs(UnmanagedType.LPStr)] System.Text.StringBuilder valueBuf,
            int bufSize);
    }
}
