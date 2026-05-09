# 🪟 GRANITE — Windows 11 Native Deployment Guide

> **Target Environment:** Windows 11 · MSVC 19.x (VS 2022 / VS 2026) · Native PowerShell (No WSL required)  
> **Performance Target:** Maximum Power — AVX2 · `/Ox` · LLVM OpenMP 3.1+ · Physical-Core Thread Saturation

> [!CAUTION]
>
> To ensure maximum performance and stability, Native Windows are currently strictly unsupported.  
> To run GRANITE, **you must use Linux or Windows Subsystem for Linux (WSL2).**   
> The engine is optimized for environments where dependencies are natively supported, allowing you to bypass installation headaches and focus on the science.

---

## Quick-Start (TL;DR)

Open a **Developer PowerShell for VS 2022** window, navigate to the repo, and run:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\setup_windows.ps1
```

This single idempotent script handles everything in Sections 1–7 below automatically.
Read the rest of this guide for manual control or troubleshooting.

---

## Prerequisites

| Tool | Version | Where to get it | Verify |
|---|---|---|---|
| Visual Studio 2022/2026 | 17.x+ | [visualstudio.microsoft.com](https://visualstudio.microsoft.com) | `cl.exe` in Dev PS |
| CMake | 3.20+ | Bundled with VS or [cmake.org](https://cmake.org/download/) | `cmake --version` |
| Git | any | [git-scm.com](https://git-scm.com) | `git --version` |
| Python | 3.10+ | [python.org](https://python.org) | `python --version` |
| MS-MPI | 10.1+ | [aka.ms/msmpi](https://aka.ms/msmpi) | Echo `$env:MSMPI_INC` |
| vcpkg | latest | bootstrapped automatically | `vcpkg version` |

> **IMPORTANT:** All PowerShell commands below must be run inside  
> **"Developer PowerShell for VS 2022"** (or `Developer Command Prompt for VS 2022`).  
> This ensures `cl.exe`, `link.exe`, and the correct Windows SDK paths are in `PATH`.

---

## Section 1 — Clone the Repository

```powershell
# Clone Granite and vcpkg as siblings (recommended layout)
git clone https://github.com/LiranOG/Granite-NR.git
git clone https://github.com/microsoft/vcpkg.git     # will sit next to Granite\

cd Granite-NR
```

---

## Section 2 — Bootstrap vcpkg and Install Dependencies

```powershell
# Bootstrap vcpkg (idempotent — safe to run again)
..\vcpkg\bootstrap-vcpkg.bat -disableMetrics

# Install required packages for x64-windows triplet
..\vcpkg\vcpkg install hdf5:x64-windows yaml-cpp:x64-windows --recurse
```

> **Clock-Skew Note:** If you see `"warning: file ... has modification time in the future"`,
> run the timestamp normalisation block in `setup_windows.ps1 -SkipBuild` to fix it idempotently.

---

## Section 3 — Install MS-MPI

1. Download both the **Runtime** and **SDK** from [https://aka.ms/msmpi](https://aka.ms/msmpi)
2. Install both installers (Runtime first, then SDK)
3. Verify the environment variables are set:

```powershell
# These should both print a valid path after installation:
echo $env:MSMPI_INC    # e.g. C:\Program Files (x86)\Microsoft SDKs\MPI\Include\
echo $env:MSMPI_LIB64  # e.g. C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64\
```

If they are blank, restart your terminal or add them manually:

```powershell
$env:MSMPI_INC   = "C:\Program Files (x86)\Microsoft SDKs\MPI\Include\"
$env:MSMPI_LIB64 = "C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64\"
```

> **No MPI?** GRANITE builds and runs correctly without MPI in single-node mode.
> Simply pass `-DGRANITE_ENABLE_MPI=OFF` to CMake.

---

## Section 4 — Set Performance Environment Variables

Run **once per terminal session** (or add to your PowerShell profile for persistence):

```powershell
# ── OpenMP Thread Saturation ──────────────────────────────────────────────────
# Query physical (non-HT) core count via WMI
$PhysicalCores = (Get-CimInstance Win32_Processor |
                  Measure-Object -Property NumberOfCores -Sum).Sum

$env:OMP_NUM_THREADS = "$PhysicalCores"   # NEVER use logical/HT count for GRMHD
$env:OMP_PROC_BIND   = "true"             # Prevents OS from migrating threads
$env:OMP_PLACES      = "cores"            # Pin to physical cores, not HT siblings

# ── HDF5 Discovery (vcpkg) ────────────────────────────────────────────────────
$env:HDF5_DIR = "..\vcpkg\installed\x64-windows\share\hdf5"

# ── MPI Paths (if MS-MPI is installed) ───────────────────────────────────────
$env:MPI_INC = $env:MSMPI_INC
$env:MPI_LIB = "$env:MSMPI_LIB64\msmpi.lib"

Write-Host "OMP_NUM_THREADS set to $env:OMP_NUM_THREADS physical cores."
```

To make these **permanent** (survives reboots):

```powershell
[System.Environment]::SetEnvironmentVariable("OMP_NUM_THREADS", "$PhysicalCores", "User")
[System.Environment]::SetEnvironmentVariable("OMP_PROC_BIND",   "true",           "User")
[System.Environment]::SetEnvironmentVariable("OMP_PLACES",      "cores",          "User")
```

---

## Section 5 — CMake Configuration (Release Build)

```powershell
# Clean any stale cache first (idempotent)
if (Test-Path build) { Remove-Item build -Recurse -Force }

# Configure — all three issues (HDF5, MPI, OpenMP) are now resolved by CMakeLists.txt
cmake -B build -S . `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_TOOLCHAIN_FILE="..\vcpkg\scripts\buildsystems\vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows `
    -DGRANITE_ENABLE_MPI=ON `
    -DGRANITE_ENABLE_OPENMP=ON `
    -DGRANITE_ENABLE_HDF5=ON `
    -DGRANITE_ENABLE_TESTS=OFF
```

> **What the upgraded CMakeLists.txt does automatically:**
> - **HDF5:** Reads `$env:HDF5_DIR` / `$env:HDF5_ROOT`, prepends to `CMAKE_PREFIX_PATH`, then tries
>   config-mode find before falling back to legacy module mode.
> - **MS-MPI:** Reads `MSMPI_INC` / `MSMPI_LIB64` (or `MPI_INC` / `MPI_LIB`) and pre-populates
>   all `MPI_*` hints before `find_package(MPI)` runs — eliminates "Compiler-NOTFOUND".
> - **OpenMP:** Injects `/openmp:llvm` on MSVC before `find_package(OpenMP)`,
>   upgrading from the legacy 2.0 vcomp stub to full OpenMP 3.1+.

---

## Section 6 — Compile

```powershell
# Build with all physical cores (fastest possible compile)
$Jobs = (Get-CimInstance Win32_Processor | Measure-Object -Property NumberOfCores -Sum).Sum
cmake --build build --config Release --parallel $Jobs
```

Expected output on success:
```
[GRANITE] OpenMP version: 3.1
[GRANITE] HDF5 1.14.x | target-mode=TRUE | parallel=FALSE
=====================================================
  GRANITE Build Configuration  v0.6.0
=====================================================
  Build type:       Release
  C++ compiler:     MSVC 19.50.xxxxx
  OpenMP:           TRUE (ver 3.1)
  MSVC Release:     /Ox /arch:AVX2 /fp:fast /GL /openmp:llvm
=====================================================
```

---

## Section 7 — Run a Simulation

### Using the Python wrapper (recommended)

```powershell
# The wrapper auto-detects physical cores and sets OMP_NUM_THREADS if not set
python scripts\run_granite.py run --benchmark B2_eq
python scripts\run_granite.py run --benchmark single_puncture
```

Expected HPC advisory (first run without env vars set):
```
[HPC] Auto-setting OMP_NUM_THREADS to 16 (Physical Core Count) for peak performance.
[HPC] Method: psutil  |  OMP_PROC_BIND=true  |  OMP_PLACES=cores
```

### Direct executable invocation (CMD or PowerShell)

```powershell
# PowerShell
.\build\bin\Release\granite_main.exe .\benchmarks\B2_eq\params.yaml

# CMD equivalent (note forward/back slash compatibility)
build\bin\Release\granite_main.exe benchmarks\B2_eq\params.yaml
```

> **Path separator note:** `granite_main.exe` accepts both `/` and `\` path separators.
> The Python wrapper (`run_granite.py`) uses `pathlib.Path` internally, which handles
> both automatically — no manual path-separator conversion needed.

---

## Section 8 — Diagnostics & Health Check

```powershell
# Full environment diagnostic
python scripts\health_check.py

# Quick thread check only
python -c "
import os, sys, subprocess
cores = (subprocess.run(['powershell','-Command',
  '(Get-CimInstance Win32_Processor | Measure-Object -Property NumberOfCores -Sum).Sum'],
  capture_output=True, text=True).stdout.strip())
omp = os.environ.get('OMP_NUM_THREADS', 'NOT SET')
print(f'Physical cores : {cores}')
print(f'OMP_NUM_THREADS: {omp}')
print('STATUS:', 'OK' if omp == cores else f'MISMATCH — set OMP_NUM_THREADS={cores}')
"
```

Expected healthy output from `health_check.py`:

```
══════════════════════════════════════════════════
       GRANITE Diagnostics & Health Check
══════════════════════════════════════════════════
 Logical  Cores (HT/SMT included)    [ OK  ] 32 detected
 Physical Cores (true compute)       [ OK  ] 16 detected  [via psutil]
 OMP_NUM_THREADS                     [ OK  ] 16 ✓ Optimal (matches physical cores)
 OMP_PROC_BIND                       [ OK  ] true
 OMP_PLACES                          [ OK  ] cores
 CMAKE_BUILD_TYPE                    [ OK  ] Release (Maximum Power)
 OpenMP Linkage                      [ OK  ] Enabled in CMake
```

---

## Section 9 — Troubleshooting Reference

### HDF5 headers not found (`HDF5_INCLUDE_DIRS-NOTFOUND`)

```powershell
# Verify vcpkg installed it
..\vcpkg\vcpkg list | Select-String hdf5

# Set the hint manually and re-configure
$env:HDF5_DIR = (Resolve-Path "..\vcpkg\installed\x64-windows\share\hdf5").Path
cmake -B build -S . --fresh -DCMAKE_TOOLCHAIN_FILE="..\vcpkg\scripts\buildsystems\vcpkg.cmake"
```

### MPI `Compiler-NOTFOUND` / `msmpi.lib` not found

```powershell
# Verify MS-MPI SDK is installed (not just Runtime)
Test-Path "C:\Program Files (x86)\Microsoft SDKs\MPI\Include\mpi.h"  # must return True

# If False: re-download SDK from https://aka.ms/msmpi and install
# Then:
$env:MSMPI_INC   = "C:\Program Files (x86)\Microsoft SDKs\MPI\Include\"
$env:MSMPI_LIB64 = "C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64\"
# Re-run CMake (the patched CMakeLists.txt reads these vars)
```

### OpenMP still reporting version 2.0

```powershell
# Confirm MSVC version (must be 19.29+ for /openmp:llvm)
cl.exe 2>&1 | Select-String "Version"
# Minimum: "Microsoft (R) C/C++ Optimizing Compiler Version 19.29"

# If older, update VS via the Visual Studio Installer
# Then: delete build\ and re-run CMake — the patched file forces /openmp:llvm
```

### Clock-skew / "file in the future" build warnings

```powershell
# Normalise all source timestamps (run once, idempotent)
Get-ChildItem src, include -Recurse -Include "*.cpp","*.hpp","*.h" |
    ForEach-Object { $_.LastWriteTime = Get-Date }
Write-Host "Timestamps updated."
```

### `python` not found / wrong Python version

```powershell
# Check which python is active
python --version
where.exe python

# If wrong version, use the full path or set via py launcher
py -3.11 scripts\run_granite.py run --benchmark B2_eq
```

---

## Section 10 — Performance Flags Quick-Reference

| Flag | Effect | Applied by |
|---|---|---|
| `/Ox` | Maximum speed (superset of `/O2`: adds `/Ot /Oy /GF /Gy`) | `CMakeLists.txt` |
| `/Ob2` | Aggressive function inlining | `CMakeLists.txt` |
| `/arch:AVX2` | Emit 256-bit SIMD + FMA3 instructions | `CMakeLists.txt` |
| `/fp:fast` | Relax IEEE 754 for FMA fusion | `CMakeLists.txt` |
| `/GL` | Whole-program optimisation (LTCG) | `CMakeLists.txt` |
| `/openmp:llvm` | LLVM runtime OpenMP 3.1+ (replaces vcomp 2.0) | `CMakeLists.txt` |
| `OMP_NUM_THREADS=N` | Use N physical cores only (no HT) | `setup_windows.ps1` / `run_granite.py` |
| `OMP_PROC_BIND=true` | Prevent thread migration between cores | `setup_windows.ps1` / `run_granite.py` |
| `OMP_PLACES=cores` | Pin threads to physical core sockets | `setup_windows.ps1` / `run_granite.py` |

---

*For Linux/macOS deployment, see [`DEPLOYMENT_AND_PERFORMANCE.md`](../user_guide/DEPLOYMENT_AND_PERFORMANCE.md).*  
*For full installation details, see [`docs/INSTALL.md`](Installation.md).*
