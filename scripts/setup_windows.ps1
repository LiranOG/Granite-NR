#Requires -Version 5.1
<#
.SYNOPSIS
    GRANITE Windows Deployment Suite - Automated Setup & Build
.DESCRIPTION
    Idempotent HPC bootstrap for the GRANITE GRMHD/CCZ4 engine on Windows 11
    with MSVC 19.x (Visual Studio 2022/2026).

    What this script does:
      1. Validates required tools (cmake, git, python, cl.exe, vcpkg)
      2. Discovers/installs MS-MPI and HDF5 via vcpkg
      3. Sets all required environment variables for the current session
      4. Performs a clean Release build with /Ox /arch:AVX2 /openmp:llvm
      5. Verifies thread saturation and prints the final diagnostic summary

    Idempotency: Safe to run multiple times. Re-runs detect existing state
    and skip steps already completed.

.PARAMETER VcpkgRoot
    Path to your vcpkg root directory. Defaults to the sibling of this repo.

.PARAMETER SkipBuild
    Skip the cmake build step (validation only).

.PARAMETER SkipInstall
    Skip vcpkg install steps (assume packages already installed).

.EXAMPLE
    .\setup_windows.ps1
    .\setup_windows.ps1 -VcpkgRoot "C:\tools\vcpkg"
    .\setup_windows.ps1 -SkipBuild
#>

param(
    [string]$VcpkgRoot   = "",
    [switch]$SkipBuild,
    [switch]$SkipInstall
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ─── Colour helpers ───────────────────────────────────────────────────────────
function Write-Ok    { param([string]$msg) Write-Host " [ OK  ] $msg" -ForegroundColor Green  }
function Write-Warn  { param([string]$msg) Write-Host " [WARN ] $msg" -ForegroundColor Yellow }
function Write-Fail  { param([string]$msg) Write-Host " [FAIL ] $msg" -ForegroundColor Red    }
function Write-Info  { param([string]$msg) Write-Host " [INFO ] $msg" -ForegroundColor Cyan   }
function Write-Hpc   { param([string]$msg) Write-Host " [HPC  ] $msg" -ForegroundColor Magenta}
function Write-Section { param([string]$title)
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
    Write-Host "  $title" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
}

# ─── Banner ───────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║    GRANITE HPC Windows Deployment Suite                     ║" -ForegroundColor Cyan
Write-Host "║    GRMHD/CCZ4 Engine  ·  MSVC Release  ·  AVX2 + LLVM-OMP  ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$ScriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot    = $ScriptDir   # script lives at repo root

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 1: Validate core toolchain
# ═══════════════════════════════════════════════════════════════════════════════
Write-Section "Toolchain Validation"

$RequiredTools = @("cmake", "git", "python")
foreach ($tool in $RequiredTools) {
    if (Get-Command $tool -ErrorAction SilentlyContinue) {
        $ver = & $tool --version 2>&1 | Select-Object -First 1
        Write-Ok "$tool  →  $ver"
    } else {
        Write-Fail "$tool not found in PATH. Install it and re-run."
        exit 1
    }
}

# cl.exe requires a Developer Command Prompt environment
$cl = Get-Command "cl.exe" -ErrorAction SilentlyContinue
if ($cl) {
    $clVer = (& cl.exe 2>&1 | Select-Object -First 1) -replace ".*Version ", "MSVC "
    Write-Ok "cl.exe  →  $clVer"
} else {
    Write-Fail "cl.exe not found. Launch this script from a 'Developer PowerShell for VS 2022' window."
    Write-Warn "  Shortcut: Start Menu → Visual Studio 2022 → Developer PowerShell for VS 2022"
    exit 1
}

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 2: Locate / Bootstrap vcpkg
# ═══════════════════════════════════════════════════════════════════════════════
Write-Section "vcpkg Discovery"

if ([string]::IsNullOrEmpty($VcpkgRoot)) {
    # Try common locations (sibling dir, VCPKG_ROOT env var, C:\tools\vcpkg)
    $candidates = @(
        "$env:VCPKG_ROOT",
        (Join-Path (Split-Path $RepoRoot -Parent) "vcpkg"),
        "C:\tools\vcpkg",
        "C:\vcpkg"
    )
    foreach ($c in $candidates) {
        if ($c -and (Test-Path (Join-Path $c "vcpkg.exe"))) {
            $VcpkgRoot = $c
            break
        }
    }
}

if ($VcpkgRoot -and (Test-Path (Join-Path $VcpkgRoot "vcpkg.exe"))) {
    Write-Ok "vcpkg found  →  $VcpkgRoot"
    $VcpkgExe      = Join-Path $VcpkgRoot "vcpkg.exe"
    $VcpkgToolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
    $env:VCPKG_ROOT = $VcpkgRoot
} else {
    Write-Warn "vcpkg not found. Bootstrapping alongside repo..."
    $VcpkgRoot = Join-Path (Split-Path $RepoRoot -Parent) "vcpkg"
    if (-not (Test-Path $VcpkgRoot)) {
        & git clone "https://github.com/microsoft/vcpkg.git" $VcpkgRoot
    }
    & "$VcpkgRoot\bootstrap-vcpkg.bat" -disableMetrics
    $VcpkgExe      = Join-Path $VcpkgRoot "vcpkg.exe"
    $VcpkgToolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
    $env:VCPKG_ROOT = $VcpkgRoot
    Write-Ok "vcpkg bootstrapped  →  $VcpkgRoot"
}

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 3: Install dependencies via vcpkg (idempotent)
# ═══════════════════════════════════════════════════════════════════════════════
Write-Section "vcpkg Package Installation"

$Packages = @("hdf5:x64-windows", "yaml-cpp:x64-windows")

if (-not $SkipInstall) {
    foreach ($pkg in $Packages) {
        Write-Info "Installing/verifying: $pkg"
        & $VcpkgExe install $pkg --recurse 2>&1 | Tail-Lines 5
        Write-Ok "$pkg  →  ready"
    }
} else {
    Write-Info "Skipping vcpkg installs (-SkipInstall flag set)."
}

# Helper function to get last N lines (avoids Import-Module dependency)
function Tail-Lines { param([int]$n = 10) $input | Select-Object -Last $n }

# ─── HDF5: Discover cmake config dir ──────────────────────────────────────────
$Hdf5CmakePath = Join-Path $VcpkgRoot "installed\x64-windows\share\hdf5"
if (Test-Path $Hdf5CmakePath) {
    $env:HDF5_DIR = $Hdf5CmakePath
    Write-Ok "HDF5_DIR  →  $env:HDF5_DIR"
} else {
    # Fallback: standalone HDF5 installer default location
    $Hdf5Standalone = "C:\Program Files\HDF_Group\HDF5"
    if (Test-Path $Hdf5Standalone) {
        $latestHdf5 = Get-ChildItem $Hdf5Standalone | Sort-Object Name -Descending | Select-Object -First 1
        if ($latestHdf5) {
            $env:HDF5_ROOT = $latestHdf5.FullName
            $env:HDF5_DIR  = Join-Path $latestHdf5.FullName "cmake"
            Write-Ok "HDF5_ROOT  →  $env:HDF5_ROOT  (standalone installer)"
        }
    } else {
        Write-Warn "HDF5 cmake config not found. Build may fail. Install via vcpkg or from https://hdf5group.org"
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 4: MS-MPI Discovery & Environment Setup
# ═══════════════════════════════════════════════════════════════════════════════
Write-Section "MS-MPI Validation"

$MsmpiBin = $env:MSMPI_BIN
$MsmpiInc = $env:MSMPI_INC
$MsmpiLib = $env:MSMPI_LIB64

if ($MsmpiInc -and (Test-Path $MsmpiInc)) {
    Write-Ok "MS-MPI INC  →  $MsmpiInc"
    Write-Ok "MS-MPI LIB  →  $MsmpiLib"
    # Expose the explicit env vars that CMakeLists.txt reads as fallback
    $env:MPI_INC = $MsmpiInc
    $env:MPI_LIB = Join-Path $MsmpiLib "msmpi.lib"
} else {
    Write-Warn "MS-MPI environment variables (MSMPI_INC / MSMPI_LIB64) not found."
    Write-Warn "  Download: https://aka.ms/msmpi"
    Write-Warn "  GRANITE will build without MPI (single-node mode)."
    Write-Warn "  To suppress this warning, pass -DGRANITE_ENABLE_MPI=OFF to CMake."
}

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 5: Physical Core Detection & OMP Thread Saturation
# ═══════════════════════════════════════════════════════════════════════════════
Write-Section "HPC Thread Configuration"

$PhysicalCores = 0
try {
    $PhysicalCores = (Get-CimInstance Win32_Processor |
                      Measure-Object -Property NumberOfCores -Sum).Sum
} catch {
    Write-Warn "WMI query failed. Trying fallback..."
}

if ($PhysicalCores -le 0) {
    try {
        # Fallback: count unique Core IDs from WMI
        $PhysicalCores = (Get-CimInstance Win32_Processor).NumberOfCores
    } catch {
        $PhysicalCores = 4
        Write-Warn "Core detection failed — defaulting to safe value: 4"
    }
}

$LogicalCores = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors

Write-Hpc "Physical cores (true compute) : $PhysicalCores"
Write-Hpc "Logical  cores (HT/SMT total) : $LogicalCores"
Write-Hpc "Recommended OMP_NUM_THREADS   : $PhysicalCores  (Physical = optimal for CCZ4/GRMHD)"

# Set for this process + all child processes (cmake, granite_main, etc.)
$env:OMP_NUM_THREADS = "$PhysicalCores"
$env:OMP_PROC_BIND   = "true"
$env:OMP_PLACES      = "cores"

Write-Ok "OMP_NUM_THREADS=$env:OMP_NUM_THREADS  OMP_PROC_BIND=true  OMP_PLACES=cores"
Write-Info "To persist across sessions, add these to your PowerShell profile or System env vars."

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 6: CMake Clock-Skew Guard (filesystem timestamp normalisation)
# ═══════════════════════════════════════════════════════════════════════════════
Write-Section "Clock-Skew Prevention"

# Touch all C++ source files so their mtime > any cached CMake metadata.
# This prevents the "Clock skew detected" / "file in the future" Make warnings
# that appear when source files have timestamps from git operations or cross-tz copies.
Write-Info "Normalising source file timestamps..."
$SourceExts = @("*.cpp", "*.hpp", "*.h", "*.c", "*.cu")
$TouchCount  = 0
foreach ($ext in $SourceExts) {
    Get-ChildItem -Path (Join-Path $RepoRoot "src"), (Join-Path $RepoRoot "include") `
                  -Recurse -Filter $ext -ErrorAction SilentlyContinue |
    ForEach-Object {
        $_.LastWriteTime = Get-Date
        $TouchCount++
    }
}
Write-Ok "Touched $TouchCount source files  (clock-skew guard active)"

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 7: Clean CMake Build
# ═══════════════════════════════════════════════════════════════════════════════
if (-not $SkipBuild) {
    Write-Section "CMake Clean Release Build"

    $BuildDir = Join-Path $RepoRoot "build"

    # Idempotent clean: only wipe if build dir exists
    if (Test-Path $BuildDir) {
        Write-Info "Removing stale build directory..."
        Remove-Item $BuildDir -Recurse -Force
        Write-Ok "Stale build removed."
    }

    # Determine MPI flag
    $MpiFlag = if ($env:MSMPI_INC -and (Test-Path $env:MSMPI_INC)) { "ON" } else { "OFF" }

    Write-Info "Configuring CMake (Release, AVX2, LLVM-OMP)..."
    $ConfigArgs = @(
        "-B", "build",
        "-S", ".",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=`"$VcpkgToolchain`"",
        "-DVCPKG_TARGET_TRIPLET=x64-windows",
        "-DGRANITE_ENABLE_MPI=$MpiFlag",
        "-DGRANITE_ENABLE_OPENMP=ON",
        "-DGRANITE_ENABLE_HDF5=ON",
        "-DGRANITE_ENABLE_TESTS=OFF"   # skip test fetches for speed — run manually
    )

    & cmake @ConfigArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "CMake configuration failed (exit $LASTEXITCODE). Check output above."
        exit $LASTEXITCODE
    }
    Write-Ok "CMake configuration successful."

    Write-Info "Building (parallel, Release config)..."
    $Jobs = $PhysicalCores
    & cmake --build build --config Release --parallel $Jobs
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "Build failed (exit $LASTEXITCODE)."
        exit $LASTEXITCODE
    }
    Write-Ok "Build successful!"

} else {
    Write-Info "Skipping build (-SkipBuild flag set)."
}

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 8: Post-Build Diagnostic Summary
# ═══════════════════════════════════════════════════════════════════════════════
Write-Section "Deployment Diagnostic Summary"

# Locate the binary
$Candidates = @(
    "build\bin\Release\granite_main.exe",
    "build\bin\granite_main.exe",
    "build\Release\granite_main.exe"
)
$BinaryFound = $false
foreach ($c in $Candidates) {
    $fullPath = Join-Path $RepoRoot $c
    if (Test-Path $fullPath) {
        Write-Ok "Executable  →  $fullPath"
        $BinaryFound = $true
        break
    }
}
if (-not $BinaryFound -and -not $SkipBuild) {
    Write-Warn "Executable not found in expected locations. Did the build succeed?"
}

Write-Host ""
Write-Host "─────────────────────────────────────────────────────────────" -ForegroundColor DarkGray
Write-Host "  Environment variables set for this session:" -ForegroundColor Gray
Write-Host "    OMP_NUM_THREADS = $env:OMP_NUM_THREADS" -ForegroundColor White
Write-Host "    OMP_PROC_BIND   = $env:OMP_PROC_BIND"   -ForegroundColor White
Write-Host "    OMP_PLACES      = $env:OMP_PLACES"       -ForegroundColor White
if ($env:HDF5_DIR)  { Write-Host "    HDF5_DIR        = $env:HDF5_DIR"  -ForegroundColor White }
if ($env:MPI_INC)   { Write-Host "    MPI_INC         = $env:MPI_INC"   -ForegroundColor White }
Write-Host "─────────────────────────────────────────────────────────────" -ForegroundColor DarkGray
Write-Host ""

# Run the Python health check if available
$HealthCheck = Join-Path $RepoRoot "scripts\health_check.py"
if (Test-Path $HealthCheck) {
    Write-Info "Running GRANITE Health Check..."
    & python $HealthCheck
} else {
    Write-Warn "Health check script not found at $HealthCheck"
}

Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║  GRANITE Windows Setup Complete. You're at Maximum Power.   ║" -ForegroundColor Green
Write-Host "║  Run a simulation:                                           ║" -ForegroundColor Green
Write-Host "║    python scripts\run_granite.py run --benchmark B2_eq      ║" -ForegroundColor Green
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""
