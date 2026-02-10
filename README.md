# FIFO Data Management System - .NET WPF Application

## Overview
This is a complete .NET 10 WPF application for managing data files using a FIFO (First In, First Out) strategy. It replaces the previous Python prototype with a modern, enterprise-grade Windows desktop application.

## Features

### 🎯 Core Functionality
- **FIFO Queue Management**: Automatically manages data files based on creation time
- **Asset Management**: Track multiple data assets with different retention policies
- **Storage Monitoring**: Monitor storage paths and capacity
- **Automated Cleanup**: Delete oldest files when storage limits are reached
- **Audit Trail**: Complete deletion log for compliance and tracking

### 🖥️ User Interface
- Modern WPF desktop application with Material Design-inspired UI
- **Control Panel**: Easy-to-use buttons for all operations
- **Three Main Views**:
  - **Assets Tab**: View and manage configured assets
  - **Data Files Tab**: Monitor all tracked files in the system
  - **Deletion Logs Tab**: Review audit trail of deleted files
- **Real-time Status Bar**: Shows operation results and system status

### 📊 Database
- SQLite database for lightweight, file-based storage
- Four main tables:
  - `assets`: Asset configurations with retention policies
  - `storage_paths`: Monitored storage locations
  - `data_files`: FIFO queue of tracked files
  - `deletion_log`: Audit trail of all deletions
- Optimized indexes for fast FIFO queries

## Architecture

```
FIFOManagement/
├── Models/
│   ├── Asset.cs              # Asset entity
│   ├── StoragePath.cs        # Storage path entity
│   ├── DataFile.cs           # File tracking entity
│   └── DeletionLog.cs        # Deletion audit entity
├── Services/
│   ├── DatabaseService.cs    # SQLite database operations
│   ├── FIFOEngine.cs         # FIFO cleanup logic
│   └── DataGenerator.cs      # Test data generation
├── ViewModels/
│   ├── ViewModelBase.cs      # MVVM base class
│   └── MainViewModel.cs      # Main window view model
├── MainWindow.xaml           # Main UI definition
└── MainWindow.xaml.cs        # Main window code-behind
```

## Technology Stack
- **.NET 10**: Latest .NET framework
- **WPF (Windows Presentation Foundation)**: Modern Windows desktop UI
- **MVVM Pattern**: Clean separation of concerns
- **SQLite**: Lightweight embedded database
- **System.Data.SQLite.Core**: SQLite ADO.NET provider

## Getting Started

### Prerequisites
- Windows 10/11
- .NET 10 SDK installed

### Building the Application
```powershell
# From the repository root
dotnet build FIFOManagement\FIFOManagement.csproj
```

### Running the Application
```powershell
# Run from Visual Studio or command line
dotnet run --project FIFOManagement\FIFOManagement.csproj
```

Or simply open `FIFOManagement.sln` in Visual Studio and press F5.

## Usage Guide

### 1. Initialize Database
Click **"🔧 Initialize Database"** to:
- Create the SQLite database
- Set up tables and indexes
- Insert sample assets (ASSET_001, ASSET_002, ASSET_003)

### 2. Generate Test Data
Click **"🧪 Generate Test Data"** to:
- Create test storage directories
- Generate sample data files for each asset
- Register files in the database
- View results in the Data Files tab

### 3. View Assets
Navigate to the **"📦 Assets"** tab to see:
- Configured assets
- Retention policies
- Priority levels

### 4. Monitor Files
Navigate to the **"📄 Data Files"** tab to:
- View all tracked files
- See file sizes and timestamps
- Monitor FIFO queue status

### 5. Run FIFO Cleanup
Click **"🗑️ Run FIFO Cleanup"** to:
- Delete the 2 oldest files from each asset
- Free up storage space
- Log deletions for audit

### 6. Review Deletion Logs
Navigate to the **"🗑️ Deletion Logs"** tab to:
- View deletion history
- See space freed
- Track who triggered deletions

## Database Location
The SQLite database is stored at:
```
<Application Directory>\Data\fifo.db
```

## Test Data Location
Generated test files are stored at:
```
<Application Directory>\TestStorage\<ASSET_ID>\
```

## Sample Assets

The system comes pre-configured with three sample assets:

1. **ASSET_001**: Sensor de Temperatura - Línea 1
   - Average size: 50 MB
   - Minimum retention: 2 hours
   - Maximum retention: 7 days
   - Priority: HIGH

2. **ASSET_002**: Monitor de Presión - Línea 2
   - Average size: 75 MB
   - Minimum retention: 4 hours
   - Maximum retention: 14 days
   - Priority: NORMAL

3. **ASSET_003**: Controlador PLC - Línea 3
   - Average size: 30 MB
   - Minimum retention: 1 hour
   - Maximum retention: 7 days
   - Priority: HIGH

## Extending the Application

### Adding New Assets
Modify `DatabaseService.InsertSampleData()` or add UI for asset creation.

### Customizing FIFO Logic
Edit `FIFOEngine.cs` to implement custom retention rules:
- Time-based deletion
- Size-based deletion
- Priority-based deletion
- Storage threshold triggers

### Adding New Features
The MVVM architecture makes it easy to add:
- Real-time monitoring
- Scheduled cleanup tasks
- Email notifications
- Dashboard analytics
- Export functionality

## Testing

The application includes built-in testing capabilities:
- Generate synthetic test data
- Simulate FIFO operations
- Verify database operations
- Test cleanup logic

## License
See LICENSE file in the repository root.

## Support
For issues or questions, please refer to the implementation guides:
- `FIFO_Data_Management_Implementation_Guide.md`
- `Guia_Implementacion_Paso_a_Paso.md`
