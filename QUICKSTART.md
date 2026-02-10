# 🚀 Quick Start Guide - FIFO Management System

## Installation & Setup

### Prerequisites
- Windows 10 or 11
- .NET 10 SDK ([Download here](https://dotnet.microsoft.com/download))

### Quick Start (Command Line)

```powershell
# 1. Navigate to project directory
cd FIFOImplementation

# 2. Build the application
dotnet build FIFOManagement\FIFOManagement.csproj

# 3. Run the application
dotnet run --project FIFOManagement\FIFOManagement.csproj
```

### Quick Start (Visual Studio)

1. Open `FIFOManagement.slnx` in Visual Studio 2022+
2. Press **F5** to build and run
3. Application window appears

## First Time Usage

### Step 1: Initialize Database
Click the **🔧 Initialize Database** button
- Creates SQLite database
- Sets up tables and indexes
- Loads 3 sample assets
- **Status**: "✅ Database initialized successfully!"

### Step 2: Generate Test Data
Click the **🧪 Generate Test Data** button
- Creates TestStorage directory
- Generates 15 test files (5 per asset)
- Registers files in database
- **Status**: "✅ Test data generated successfully!"

### Step 3: View Your Data
Navigate through tabs:
- **📦 Assets**: See 3 configured assets
- **📄 Data Files**: View 15 generated files
- **🗑️ Deletion Logs**: Currently empty

### Step 4: Test FIFO Cleanup
Click the **🗑️ Run FIFO Cleanup** button
- Deletes 6 oldest files (2 per asset)
- **Status**: "✅ FIFO Cleanup: Deleted 6 oldest files"

### Step 5: Verify Results
- **📄 Data Files**: Now shows 9 files
- **🗑️ Deletion Logs**: Shows 6 deletion records

## Common Operations

### Load/Refresh Data
Click **📊 Load Data** to refresh all views with current database state

### Reset Everything
1. Close application
2. Delete `Data\fifo.db`
3. Delete `TestStorage\` directory
4. Reopen application
5. Click **🔧 Initialize Database**

### Monitor Status
Watch the bottom status bar for:
- ✅ Success messages
- ⚠️ Warnings
- ❌ Error messages

## Understanding the UI

### Control Panel (Top)
Four buttons for main operations:
- **Green** = Setup/Initialize
- **Blue** = Load/Refresh
- **Orange** = Test/Generate
- **Red** = Delete/Cleanup

### Tabs (Middle)
Three data views:
- **Assets**: Configuration data
- **Data Files**: Active FIFO queue
- **Deletion Logs**: Audit history

### Status Bar (Bottom)
Real-time feedback on operations

## Sample Assets

The system includes 3 pre-configured assets:

| Asset ID | Name | Avg Size | Min Retention | Max Retention | Priority |
|----------|------|----------|---------------|---------------|----------|
| ASSET_001 | Sensor de Temperatura | 50 MB | 2 hours | 7 days | HIGH |
| ASSET_002 | Monitor de Presión | 75 MB | 4 hours | 14 days | NORMAL |
| ASSET_003 | Controlador PLC | 30 MB | 1 hour | 7 days | HIGH |

## Typical Workflow

```
1. Initialize Database
   ↓
2. Generate Test Data
   ↓
3. View Data Files (15 files)
   ↓
4. Run FIFO Cleanup (deletes oldest)
   ↓
5. Check Deletion Logs (audit trail)
   ↓
6. Load Data (refresh view)
```

## File Locations

### Database
```
FIFOManagement\bin\Debug\net10.0-windows\Data\fifo.db
```

### Test Files
```
FIFOManagement\bin\Debug\net10.0-windows\TestStorage\
├── ASSET_001\
├── ASSET_002\
└── ASSET_003\
```

## Troubleshooting

### "Please initialize database first"
**Solution**: Click **🔧 Initialize Database** button

### No files in Data Files tab
**Solution**: Click **🧪 Generate Test Data** button

### Build errors
**Solution**: 
```powershell
dotnet restore FIFOManagement\FIFOManagement.csproj
dotnet build FIFOManagement\FIFOManagement.csproj
```

### Application won't start
**Solution**: 
1. Check .NET 10 SDK is installed: `dotnet --version`
2. Should show: `10.0.102` or similar
3. If not, download from [dotnet.microsoft.com](https://dotnet.microsoft.com)

## Next Steps

### For Testing
- Generate more test data
- Run multiple cleanup cycles
- Observe deletion patterns

### For Development
- Review code in `Services\` folder
- Customize FIFO logic in `FIFOEngine.cs`
- Add new features in `MainViewModel.cs`
- Modify UI in `MainWindow.xaml`

### For Production
- Configure real storage paths
- Add real assets
- Customize retention policies
- Implement scheduled cleanup
- Add monitoring/alerting

## Getting Help

### Documentation
- `README.md` - Full feature documentation
- `PROJECT_SUMMARY.md` - Technical overview
- `FIFO_Data_Management_Implementation_Guide.md` - Implementation details
- `Guia_Implementacion_Paso_a_Paso.md` - Step-by-step guide (Spanish)

### Source Code
All code is in `FIFOManagement\` directory:
- **Models**: Data structures
- **Services**: Business logic
- **ViewModels**: UI logic
- **XAML**: User interface

## Success Indicators

✅ Database initialized  
✅ Test data generated  
✅ Files visible in grid  
✅ Cleanup deletes files  
✅ Logs show deletions  
✅ Status bar shows success messages  

---

**You're ready to go! Start with "🔧 Initialize Database"** 🎉
