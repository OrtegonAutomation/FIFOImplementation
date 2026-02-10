using System;
using System.IO;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using FIFOManagement.Interop;
using FIFOManagement.Services;

namespace FIFOManagement.ViewModels
{
    public class MainViewModel : ViewModelBase
    {
        private readonly EngineService _engine = new();

        // Config
        private string _rootPath = "";
        private double _storageLimitGB = 2.0;
        private int _granularity = FIFOGranularity.AssetIndex;
        private int _scheduleHour = 3;
        private int _scheduleMinute = 0;
        private double _testDataSizeGB = 2.0;

        // State
        private string _statusMessage = "Ready — Initialize engine to begin";
        private bool _isInitialized;
        private bool _isExecuting;
        private bool _isScheduleActive;
        private int _progressPercent;

        // Forecast dashboard
        private double _currentStorageMB;
        private double _forecastMB;
        private double _growthRateMB;
        private double _usagePct;
        private int _historyDays;
        private string _actionLabel = "—";
        private string _lastRun = "Never";
        private string _nextRun = "—";
        private int _lastFilesDeleted;
        private double _lastMBFreed;

        // Properties — Config
        public string RootPath { get => _rootPath; set => SetProperty(ref _rootPath, value); }
        public double StorageLimitGB { get => _storageLimitGB; set => SetProperty(ref _storageLimitGB, value); }
        public int Granularity { get => _granularity; set => SetProperty(ref _granularity, value); }
        public int ScheduleHour { get => _scheduleHour; set => SetProperty(ref _scheduleHour, value); }
        public int ScheduleMinute { get => _scheduleMinute; set => SetProperty(ref _scheduleMinute, value); }
        public double TestDataSizeGB { get => _testDataSizeGB; set => SetProperty(ref _testDataSizeGB, value); }

        // Properties — State
        public string StatusMessage { get => _statusMessage; set => SetProperty(ref _statusMessage, value); }
        public bool IsInitialized { get => _isInitialized; set { SetProperty(ref _isInitialized, value); CommandManager.InvalidateRequerySuggested(); } }
        public bool IsExecuting { get => _isExecuting; set { SetProperty(ref _isExecuting, value); CommandManager.InvalidateRequerySuggested(); } }
        public bool IsScheduleActive { get => _isScheduleActive; set { SetProperty(ref _isScheduleActive, value); CommandManager.InvalidateRequerySuggested(); } }
        public int ProgressPercent { get => _progressPercent; set => SetProperty(ref _progressPercent, value); }

        // Properties — Dashboard
        public double CurrentStorageMB { get => _currentStorageMB; set => SetProperty(ref _currentStorageMB, value); }
        public double ForecastMB { get => _forecastMB; set => SetProperty(ref _forecastMB, value); }
        public double GrowthRateMB { get => _growthRateMB; set => SetProperty(ref _growthRateMB, value); }
        public double UsagePct { get => _usagePct; set => SetProperty(ref _usagePct, value); }
        public int HistoryDays { get => _historyDays; set => SetProperty(ref _historyDays, value); }
        public string ActionLabel { get => _actionLabel; set => SetProperty(ref _actionLabel, value); }
        public string LastRun { get => _lastRun; set => SetProperty(ref _lastRun, value); }
        public string NextRun { get => _nextRun; set => SetProperty(ref _nextRun, value); }
        public int LastFilesDeleted { get => _lastFilesDeleted; set => SetProperty(ref _lastFilesDeleted, value); }
        public double LastMBFreed { get => _lastMBFreed; set => SetProperty(ref _lastMBFreed, value); }

        // Commands
        public ICommand InitializeCommand { get; }
        public ICommand BrowsePathCommand { get; }
        public ICommand ExecuteNowCommand { get; }
        public ICommand GenerateTestDataCommand { get; }
        public ICommand ToggleScheduleCommand { get; }

        public MainViewModel()
        {
            InitializeCommand = new RelayCommand(InitializeEngine, () => !IsInitialized && !IsExecuting);
            BrowsePathCommand = new RelayCommand(BrowsePath);
            ExecuteNowCommand = new RelayCommand(ExecuteNow, () => IsInitialized && !IsExecuting);
            GenerateTestDataCommand = new RelayCommand(GenerateTestData, () => IsInitialized && !IsExecuting);
            ToggleScheduleCommand = new RelayCommand(ToggleSchedule, () => IsInitialized && !IsExecuting);
        }

        private async void InitializeEngine()
        {
            try
            {
                string dbDir = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Data");
                Directory.CreateDirectory(dbDir);
                string dbPath = Path.Combine(dbDir, "fifo.db");

                StatusMessage = "Initializing C++ engine...";
                int rc = _engine.Initialize(dbPath);
                if (rc != FIFOError.OK)
                {
                    StatusMessage = $"Engine init failed (code {rc})";
                    return;
                }

                // Load saved config
                string savedRoot = _engine.GetConfig("root_path");
                if (!string.IsNullOrEmpty(savedRoot)) RootPath = savedRoot;
                string savedLimit = _engine.GetConfig("storage_limit_gb");
                if (double.TryParse(savedLimit, System.Globalization.NumberStyles.Any,
                    System.Globalization.CultureInfo.InvariantCulture, out double lim))
                    StorageLimitGB = lim;
                string savedGran = _engine.GetConfig("granularity");
                if (int.TryParse(savedGran, out int g)) Granularity = g;

                IsInitialized = true;
                StatusMessage = "Engine initialized — Configure root path and execute";
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error: {ex.Message}";
            }
        }

        private void BrowsePath()
        {
            var dlg = new Microsoft.Win32.OpenFolderDialog { Title = "Select Root Storage Path" };
            if (dlg.ShowDialog() == true)
                RootPath = dlg.FolderName;
        }

        private async void ExecuteNow()
        {
            if (string.IsNullOrWhiteSpace(RootPath))
            {
                StatusMessage = "Please set a root path first";
                return;
            }

            IsExecuting = true;
            StatusMessage = "Executing pipeline: Scan → Forecast → Evaluate → Cleanup...";
            try
            {
                SaveConfig();
                double limitMb = StorageLimitGB * 1024.0;
                var result = await _engine.ExecuteFullAsync(RootPath, Granularity, limitMb, 0.70);
                UpdateDashboard(result);
                StatusMessage = $"Pipeline complete — {FIFOAction.ToLabel(result.Action)} | " +
                                $"Deleted {result.FilesDeleted} files ({result.MBFreed:F1} MB freed)";
            }
            catch (EngineException ex)
            {
                StatusMessage = $"Engine error: {ex.Message}";
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error: {ex.Message}";
            }
            finally
            {
                IsExecuting = false;
            }
        }

        private async void GenerateTestData()
        {
            if (string.IsNullOrWhiteSpace(RootPath))
            {
                StatusMessage = "Please set a root path first";
                return;
            }

            IsExecuting = true;
            ProgressPercent = 0;
            StatusMessage = "Generating test data...";
            try
            {
                SaveConfig();
                int rc = await _engine.GenerateTestDataAsync(RootPath, TestDataSizeGB, (pct, msg) =>
                {
                    Application.Current.Dispatcher.Invoke(() =>
                    {
                        ProgressPercent = pct;
                        StatusMessage = $"Generating: {msg} ({pct}%)";
                    });
                });

                if (rc == FIFOError.OK)
                    StatusMessage = $"Test data generated ({TestDataSizeGB:F1} GB) in {RootPath}";
                else
                    StatusMessage = $"Data generation failed (code {rc})";
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error: {ex.Message}";
            }
            finally
            {
                IsExecuting = false;
                ProgressPercent = 0;
            }
        }

        private void ToggleSchedule()
        {
            if (IsScheduleActive)
            {
                _engine.StopSchedule();
                IsScheduleActive = false;
                NextRun = "—";
                StatusMessage = "Schedule stopped";
            }
            else
            {
                if (string.IsNullOrWhiteSpace(RootPath))
                {
                    StatusMessage = "Please set a root path first";
                    return;
                }
                SaveConfig();
                double limitMb = StorageLimitGB * 1024.0;
                int rc = _engine.StartSchedule(RootPath, Granularity, limitMb, 0.70, ScheduleHour, ScheduleMinute);
                if (rc == FIFOError.OK)
                {
                    IsScheduleActive = true;
                    RefreshStatus();
                    StatusMessage = $"Schedule started — Daily at {ScheduleHour:D2}:{ScheduleMinute:D2}";
                }
                else
                {
                    StatusMessage = $"Failed to start schedule (code {rc})";
                }
            }
        }

        private void UpdateDashboard(FullResult r)
        {
            CurrentStorageMB = r.CurrentMB;
            ForecastMB = r.PredictedMB;
            GrowthRateMB = r.GrowthRate;
            UsagePct = r.UsagePct;
            HistoryDays = r.HistoryDays;
            ActionLabel = FIFOAction.ToLabel(r.Action);
            LastFilesDeleted = r.FilesDeleted;
            LastMBFreed = r.MBFreed;
            LastRun = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
        }

        private void RefreshStatus()
        {
            try
            {
                var info = _engine.GetStatus();
                if (!string.IsNullOrEmpty(info.LastRun)) LastRun = info.LastRun;
                if (!string.IsNullOrEmpty(info.NextRun)) NextRun = info.NextRun;
                IsScheduleActive = info.IsScheduled != 0;
            }
            catch { }
        }

        private void SaveConfig()
        {
            if (!_engine.IsInitialized) return;
            _engine.SetConfig("root_path", RootPath);
            _engine.SetConfig("storage_limit_gb",
                StorageLimitGB.ToString(System.Globalization.CultureInfo.InvariantCulture));
            _engine.SetConfig("granularity", Granularity.ToString());
        }
    }

    public class RelayCommand : ICommand
    {
        private readonly Action _execute;
        private readonly Func<bool>? _canExecute;

        public RelayCommand(Action execute, Func<bool>? canExecute = null)
        {
            _execute = execute ?? throw new ArgumentNullException(nameof(execute));
            _canExecute = canExecute;
        }

        public event EventHandler? CanExecuteChanged
        {
            add { CommandManager.RequerySuggested += value; }
            remove { CommandManager.RequerySuggested -= value; }
        }

        public bool CanExecute(object? parameter) => _canExecute?.Invoke() ?? true;
        public void Execute(object? parameter) => _execute();
    }
}
