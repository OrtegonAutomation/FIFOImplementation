$ErrorActionPreference = "Stop"

# Paths
$sdkBase = "C:\Program Files\Microsoft Visual Studio\18\Community\SDK\ScopeCppSDK\vc15"
$vcBin   = "$sdkBase\VC\bin"
$vcInc   = "$sdkBase\VC\include"
$vcLib   = "$sdkBase\VC\lib"
$ucrtInc = "$sdkBase\SDK\include\ucrt"
$umInc   = "$sdkBase\SDK\include\um"
$sharedInc = "$sdkBase\SDK\include\shared"
$sdkLib  = "$sdkBase\SDK\lib"

$env:PATH = "$vcBin;$env:PATH"
$env:INCLUDE = "$vcInc;$ucrtInc;$umInc;$sharedInc"
$env:LIB = "$vcLib;$sdkLib"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$engineDir = Join-Path $projectRoot "FIFOEngine"
$buildDir  = Join-Path $engineDir "build"
$wpfBin    = Join-Path $projectRoot "FIFOManagement\bin\Debug\net10.0-windows"

# Create build directory
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Building FIFO Engine (C++ DLL)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Source files
$sources = @(
    "$engineDir\third_party\sqlite3.c",
    "$engineDir\src\database.cpp",
    "$engineDir\src\scanner.cpp",
    "$engineDir\src\forecast.cpp",
    "$engineDir\src\cleanup.cpp",
    "$engineDir\src\datagen.cpp",
    "$engineDir\src\scheduler.cpp",
    "$engineDir\src\fifo_api.cpp"
)

$includes = "/I`"$engineDir\include`" /I`"$engineDir\third_party`" /I`"$engineDir\src`""
$defines = "/DFIFO_ENGINE_EXPORTS /DSQLITE_THREADSAFE=1 /D_CRT_SECURE_NO_WARNINGS"
$flags = "/nologo /O2 /EHsc /std:c++14 /W3 /utf-8 /MD"

# Step 1: Compile each source to .obj
Write-Host "`nCompiling sources..." -ForegroundColor Yellow
$objFiles = @()
foreach ($src in $sources) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($src)
    $obj = Join-Path $buildDir "$name.obj"
    $objFiles += $obj

    $isC = $src.EndsWith(".c")
    $langFlag = if ($isC) { "/TC" } else { "/TP /std:c++14" }

    Write-Host "  Compiling $name..." -NoNewline
    $args = "$flags $langFlag $defines $includes /c `"$src`" /Fo`"$obj`""
    $proc = Start-Process -FilePath "$vcBin\cl.exe" -ArgumentList $args -NoNewWindow -Wait -PassThru -RedirectStandardError "$buildDir\$name.err"
    if ($proc.ExitCode -ne 0) {
        Write-Host " FAILED" -ForegroundColor Red
        Get-Content "$buildDir\$name.err"
        exit 1
    }
    Write-Host " OK" -ForegroundColor Green
}

# Step 2: Link DLL
Write-Host "`nLinking fifo_engine.dll..." -NoNewline
$dllPath = Join-Path $buildDir "fifo_engine.dll"
$objList = ($objFiles | ForEach-Object { "`"$_`"" }) -join " "
$linkLibs = "kernel32.lib advapi32.lib shell32.lib"
$linkArgs = "/nologo /DLL /OUT:`"$dllPath`" $objList $linkLibs"
$proc = Start-Process -FilePath "$vcBin\link.exe" -ArgumentList $linkArgs -NoNewWindow -Wait -PassThru -RedirectStandardError "$buildDir\link.err"
if ($proc.ExitCode -ne 0) {
    Write-Host " FAILED" -ForegroundColor Red
    Get-Content "$buildDir\link.err"
    exit 1
}
Write-Host " OK" -ForegroundColor Green

# Step 3: Copy DLL to WPF output
Write-Host "`nCopying DLL to WPF output..." -NoNewline
New-Item -ItemType Directory -Path $wpfBin -Force | Out-Null
Copy-Item $dllPath $wpfBin -Force
Write-Host " OK" -ForegroundColor Green

# Step 4: Build WPF project
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host " Building WPF Application (C# .NET)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Push-Location $projectRoot
dotnet build FIFOManagement\FIFOManagement.csproj -c Debug --nologo -v q
Pop-Location

Write-Host "`n========================================" -ForegroundColor Green
Write-Host " Build Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host "DLL: $dllPath"
Write-Host "WPF: $wpfBin"
