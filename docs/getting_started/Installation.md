# 🌌 GRANITE Installation & Troubleshooting Guide

This guide provides complete, step-by-step instructions for setting up **GRANITE** from scratch on any system.
Navigate to your platform below, follow every step in order, and run the final build command.


> ### 🚀 **Welcome to the GRANITE Installation Guide!**
> 
> To ensure maximum performance and stability, running GRANITE currently requires **Linux** or **Windows Subsystem** for Linux **(WSL2)**.
> 
> Why the restriction? The engine has been heavily optimized for environments where its dependencies are natively and seamlessly supported. My primary goal is to allow you to completely bypass frustrating installation headaches, dependency conflicts, and configuration loops, so you can focus entirely on what matters most: the science.
> 
> #### **My promise to you**: This OS restriction is temporary. In the future, once the core engine is completely flawless, battle-tested, and perfectly optimized, I will dedicate the necessary time to build native cross-platform support. GRANITE will eventually become a true, seamless **plug-and-play** experience across every operating system. Until then, the setups below will provide the most powerful and stable experience.


---

## 📋 General Requirements

Every platform requires the following tools before installation:

| Tool | Minimum Version | Purpose |
| :--- | :--- | :--- |
| **Git** | 2.30+ | Clone the repository |
| **Python** | 3.8+ | Run `run_granite.py` and analysis scripts |
| **CMake** | 3.20+ | Configure and compile the C++ engine |
| **C++ Compiler** | GCC 12+ / Clang 15+ / MSVC 2022+ | Build the simulation binary |
| **HDF5** (dev headers) | 1.12+ | Parallel I/O for simulation checkpoints |
| **OpenMPI** (dev headers) | 4.0+ | Multi-rank parallel execution |
| **yaml-cpp** (dev headers) | 0.7+ | YAML configuration file parsing |

> [!IMPORTANT]
> You **must** install all components above before running `python scripts/run_granite.py build`.
> Missing any one of them will cause the build to fail.

---

## 🪟 Windows Setup


⚙️ Windows Installation (via WSL2)
> Windows users, you're in the right place!
>
> While GRANITE is built for Linux, you don't need to dual-boot or run sluggish virtual machines.
> By using WSL2 (Windows Subsystem for Linux), you get the full, native performance of Linux directly within your familiar Windows environment.
> 
> The guide below will walk you through setting up WSL2 and getting GRANITE running smoothly in just a few steps.

Windows users have three supported paths. **WSL2 is strongly recommended** for best compatibility.

---

### Option A — WSL2 + Ubuntu (Recommended ⭐)

WSL2 provides a full Linux environment inside Windows — the easiest path for HPC tools.

**Step 1: Enable WSL2**
Open PowerShell **as Administrator** and run:
```powershell
wsl --install
```
Restart your PC when prompted. Ubuntu will be installed automatically.

**Step 2: Open an Ubuntu terminal, then install all dependencies:**
```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y git python3 python3-pip build-essential cmake \
    libhdf5-dev libopenmpi-dev libyaml-cpp-dev
```

**Step 3: Clone & Build**
```bash
git clone https://github.com/LiranOG/Granite-NR.git
cd Granite-NR
python3 scripts/run_granite.py build
```

**Step 4: Run a benchmark simulation:**
```bash
python3 scripts/run_granite.py run --benchmark single_puncture
```

---

### Option B — Miniforge / Anaconda / Conda (Cross-Platform)

Use this if you work inside a **Miniforge Prompt**, **Anaconda Prompt**, or any Conda environment.

**Step 1: Create a dedicated GRANITE environment**
```bash
conda create -n granite_env python=3.10
conda activate granite_env
```

**Step 2: Install all build tools and dependencies via conda-forge**
```bash
conda install -c conda-forge cmake hdf5 openmpi yaml-cpp git
```

> [!IMPORTANT]
> On **Windows Miniforge**, the C/C++ compiler must come from Visual Studio Build Tools.
> Install "[Visual Studio Build Tools 2022](https://visualstudio.microsoft.com/visual-cpp-build-tools/)"
> and select the **"Desktop development with C++"** workload. CMake will detect it automatically.

**Step 3: Clone & Build**
```bash
git clone https://github.com/LiranOG/Granite-NR.git
cd Granite-NR
python scripts/run_granite.py build
```

**Step 4: Run a benchmark simulation:**
```bash
python scripts/run_granite.py run --benchmark single_puncture
```

---

### Option C — Native Windows (PowerShell / CMD / VS Code / Node.js Prompt)

Use this only if you cannot use WSL2 or Conda. More manual setup is required.

**Step 1: Install Git for Windows**
Download and install from [git-scm.com](https://git-scm.com/downloads). Accept all default options.

**Step 2: Install CMake**
Download the Windows installer (`.msi`) from [cmake.org/download](https://cmake.org/download/).
During installation, select: **"Add CMake to the system PATH for all users"**.

**Step 3: Install Visual Studio Build Tools**
Download from [visualstudio.microsoft.com/visual-cpp-build-tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/).
Select the **"Desktop development with C++"** workload. This installs the MSVC compiler (cl.exe).

**Step 4: Install Python**
Download from [python.org](https://www.python.org/downloads/). During installation check: **"Add Python to PATH"**.

> [!IMPORTANT]
> To compile natively on Windows, you **must** use `vcpkg` to manage C++ dependencies.

**Step 5: Install vcpkg & Dependencies**
```powershell
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install hdf5 mpi yaml-cpp
cd ..
```

**Step 6: Clone & Build** (in PowerShell or CMD)
```powershell
git clone https://github.com/LiranOG/Granite-NR.git
cd Granite-NR
# Tell CMake where vcpkg is located so it finds the packages natively
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="../vcpkg/scripts/buildsystems/vcpkg.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

**Step 7: Run a benchmark simulation:**
```powershell
python scripts/run_granite.py run --benchmark single_puncture
```

---

## 🐧 Linux Setup

---

### Ubuntu / Debian

**Step 1: Install all dependencies**
```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y git python3 python3-pip build-essential cmake \
    libhdf5-dev libopenmpi-dev libyaml-cpp-dev
```

**Step 2: Clone & Build**
```bash
git clone https://github.com/LiranOG/Granite-NR.git
cd Granite-NR
python3 scripts/run_granite.py build
```

**Step 3: Run a benchmark simulation:**
```bash
python3 scripts/run_granite.py run --benchmark single_puncture
```

---

### Fedora / RHEL / CentOS / Rocky Linux

**Step 1: Install all dependencies**
```bash
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y git python3 cmake hdf5-devel openmpi-devel yaml-cpp-devel
```

**Step 2: Load the MPI module** (required on RHEL/Rocky before building)
```bash
module load mpi/openmpi-x86_64
```

**Step 3: Clone & Build**
```bash
git clone https://github.com/LiranOG/Granite-NR.git
cd Granite-NR
python3 scripts/run_granite.py build
```

**Step 4: Run a benchmark simulation:**
```bash
python3 scripts/run_granite.py run --benchmark single_puncture
```

---

## 🍎 macOS Setup


🍏 macOS Installation
> Welcome, Mac users!
> 
> Since macOS is a Unix-based operating system, there is a solid foundation to work with. However, because GRANITE is fundamentally optimized for Linux architectures, setting it up on macOS requires a few extra steps to bridge the gap (such as configuring specific dependencies or using containerization).
> 
> Don't worry—this section will guide you through the necessary tweaks to get the engine successfully compiled and running on your machine.


**Step 1: Install Homebrew** (if not already installed)
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

**Step 2: Install all dependencies**
```bash
brew install git python3 cmake hdf5 open-mpi yaml-cpp
```

**Step 3: Clone & Build**
```bash
git clone https://github.com/LiranOG/Granite-NR.git
cd Granite-NR
python3 scripts/run_granite.py build
```

**Step 4: Run a benchmark simulation:**
```bash
python3 scripts/run_granite.py run --benchmark single_puncture
```

---

## 🔧 All Available Commands

```bash
# Build the project (compile C++ engine)
python3 scripts/run_granite.py build

# Run any benchmark simulation
python3 scripts/run_granite.py run --benchmark single_puncture
python3 scripts/run_granite.py run --benchmark B5_star

# Format all C++ source files (requires clang-format)
python3 scripts/run_granite.py format

# Remove all build artifacts and start fresh
python3 scripts/run_granite.py clean
```

> [!NOTE]
> On **Windows** (PowerShell/CMD), use `python` instead of `python3`.
> On **WSL2, Linux, and macOS**, use `python3`.

---

## 🧪 Testing & Validation

After a successful build, always verify the engine is working correctly by running the tests.

### Step 1 — Pre-Flight Diagnostic (Health Check)

Ensure your build hit the required "Maximum Power" targets and your system CPU thread saturation is properly configured.

```bash
python scripts/health_check.py
```
*(On WSL2/macOS/Ubuntu use `python3`)*

### Step 2 — Unit Test Suite (107 tests)

```bash
# Option A — run directly from the project root (recommended)
build/bin/granite_tests

# Option B — run via CTest with verbose output
# ⚠️  WARNING: This changes your directory into build/. Run 'cd ..' afterward!
cd build && ctest --output-on-failure -j4
cd ..   # ← IMPORTANT: return to project root before the next step!
```

**Expected output:**
```
[==========] 107 tests from 20 test suites ran.
[  PASSED  ] 105 tests.
[  SKIPPED ] 2 tests.
```

> [!IMPORTANT]
> If you used `cd build && ctest`, you **must** run `cd ..` to return to the project root before continuing. All subsequent commands (`granite_analysis.cli.dev_benchmark`, `run_granite.py`) must be run from the project root — not from inside `build/`.

If any test fails, check the error message carefully — the test name tells you which physics module is affected.

---

### Step 2 — Developer Diagnostic Benchmark ⭐

> [!IMPORTANT]
> **You must be in the project root directory**  to run this. If you used `cd build` earlier, get back with:
> ```bash
> cd ..   # or: cd ~/Granite  (use your actual project path)
> ```
> Confirm you are in the right place: `ls python/granite_analysis/` should show the package directory.

> [!NOTE]
> **First-time setup — virtual environment required on modern Linux/WSL2:**
> Ubuntu 22.04+ and Debian 12+ enforce [PEP 668](https://peps.python.org/pep-0668/) which
> blocks system-wide `pip install`. You **must** use a virtual environment:
> ```bash
> # Run once from the project root
> sudo apt install python3.12-venv   # Ubuntu 24.04 / WSL2 / Debian — required
> python3 -m venv .venv
> source .venv/bin/activate      # activate every new terminal session
> pip install -e .[dev]
>
> # Verify the install worked
> python -m granite_analysis.cli.dev_benchmark --help
> ```
> The `.venv/` directory is already listed in `.gitignore` — it will not be committed.
> **Windows (Conda):** activate your Conda environment first, then `pip install -e .[dev]`.

The `granite_analysis` package is the **primary validation tool** for the full engine. Invoke it via the integrated dev pipeline or directly:

| Diagnostic | What it measures | Healthy range |
|:---|:---|:---|
| `α_center` (lapse) | Gauge collapse & recovery at the puncture | 1.0 → 0.003 → 0.03+ |
| `‖H‖₂` (Hamiltonian constraint) | Constraint satisfaction across the grid | < 1.0 through t=6M |
| NaN forensics | Which variable, where, propagation speed | "No NaN events detected" |
| Advection CFL | `\|β\|·dt/dx` — shift-driven CFL number | < 1.0 (warning if > 1) |
| Phase classification | Trumpet forming / Lapse recovering / CRASHED | Should reach "Lapse recovering" |

**Running the benchmark:**

> **Important:** Run this from the **project root directory** . `cd ~/Granite` before running if you are not already there.

```bash
# Integrated pipeline — builds, runs, and streams to the live dashboard
python3 scripts/run_granite.py dev

# Direct invocation — pipe an existing log or stream live from the engine
python3 -m granite_analysis.cli.dev_benchmark

# Export telemetry to structured files
python3 -m granite_analysis.cli.dev_benchmark --json results.json --csv results.csv

# Suppress terminal output (CI/scripted mode)
python3 -m granite_analysis.cli.dev_benchmark --quiet --json results.json

# Watch mode — auto-rebuild and restart on any src/ change
python3 scripts/run_granite.py dev --watch
```

**Healthy output reference:**

```
  step=10   t=0.6250M   α_center=0.01270  [DEEP]      ‖H‖₂=0.316  [OK]
  step=20   t=1.2500M   α_center=0.00562  [DEEP]      ‖H‖₂=0.114  [OK]
  step=30   t=1.8750M   α_center=0.00486  [COLLAPSE]  ‖H‖₂=0.106  [OK]
  step=40   t=2.5000M   α_center=0.00655  [DEEP]      ‖H‖₂=0.119  [OK]
  step=50   t=3.1250M   α_center=0.01068  [DEEP]      ‖H‖₂=0.135  [OK]
  step=60   t=3.7500M   α_center=0.01689  [DEEP]      ‖H‖₂=0.150  [OK]
  step=70   t=4.3750M   α_center=0.02393  [RECOVER]   ‖H‖₂=0.157  [OK]
  step=80   t=5.0000M   α_center=0.02903  [RECOVER]   ‖H‖₂=0.153  [OK]
```

A simulation reaching `step=80` with `‖H‖₂ < 1.0` and `α_center` recovering confirms the full evolution pipeline is working correctly.

**What the phases mean:**

| Phase label | Physics | Expected? |
|:---|:---|:---|
| `COLLAPSE` | Lapse α collapsing toward 0 as the gauge wave leaves | ✅ Normal (t ≈ 0.5–2M) |
| `DEEP` | Lapse floored near puncture, trumpet forming | ✅ Normal (t ≈ 1–4M) |
| `RECOVER` | Trumpet stable, α recovering — 1+log slicing working | ✅ Target state |
| `CRASHED (NaN)` | NaN detected — indicates a physics or numerical bug | ❌ Investigate |

**If the benchmark crashes immediately (step < 10 with NaN):**

```bash
# Run with verbose NaN forensics (pipe from the engine or from a log file)
python3 -m granite_analysis.cli.dev_benchmark --quiet
python3 -m granite_analysis.cli.sim_tracker my_run.log
```

The NaN forensics report will show:
- **Variable name** (e.g. `A_XX = Ã_xx`, `CHI = χ`, `LAPSE = α`)
- **Grid location** `(i, j, k)`
- **Propagation speed** in cells/step

This tells you which physics module or numerical scheme is responsible.

**Log files:**

All benchmark runs are automatically logged to:
```
dev_logs/dev_benchmark_YYYYMMDD_HHMMSS.log
```

Export structured telemetry directly from the CLI:
```bash
python3 -m granite_analysis.cli.dev_benchmark --json results.json --csv results.csv
```

These logs contain the complete step-by-step output and the final summary table — useful for comparing results across code changes.

---

### Step 3 — Known Physical Limitation

> [!NOTE]
> The `single_puncture` benchmark is **expected** to develop constraint growth starting at `t ≈ 5.6M` and crash at `t ≈ 6.25M`. This is **not a bug** — it is a domain-size limitation: the gauge wave (speed √2 for 1+log slicing) from the puncture reaches the outer boundary (`r = 8M`) at `t = 8M/√2 ≈ 5.66M`. The simulation is physically correct up to that point.
>
> To extend the stable evolution time, increase the domain size in `benchmarks/single_puncture/params.yaml`:
> - `domain_lo: [-16, -16, -16]` + `domain_hi: [16, 16, 16]` → gauge wave arrives at t ≈ 11.3M  
> - `domain_lo: [-32, -32, -32]` + `domain_hi: [32, 32, 32]` → gauge wave arrives at t ≈ 22.6M
>
> (Increase the grid resolution accordingly to maintain accuracy near the puncture.)



---

## ❓ Q&A / Troubleshooting

---

### 🛑 `remote: Repository not found.` / `fatal: repository ... not found`
**Reason**: The repository URL is wrong. The original README had an outdated URL.
**Solution**: Always use the exact command:
```bash
git clone https://github.com/LiranOG/Granite-NR.git
```
Do not change the username (`LiranOG`) or the repository name (`Granite-NR`). Case matters.

---

### 🛑 `cd: The system cannot find the path specified.`
**Reason**: The `git clone` failed (see question above), so the folder was never created.
**Solution**:
1. Confirm the clone succeeded: the output should say `Resolving deltas: 100%`.
2. Check what folders exist: run `dir` (Windows) or `ls` (Linux/Mac).
3. Then run `cd Granite` (capital G — the folder name matches the repo name).

---

### 🛑 `Error: 'cmake' is not installed or not available in PATH.`
**Reason**: CMake is not installed, or was installed but not added to your system PATH.
**Solution by environment:**
- **Windows (native)**: Download the CMake `.msi` installer from [cmake.org](https://cmake.org/download/) and check **"Add CMake to PATH"** during setup. Restart your terminal.
- **Miniforge/Conda**: Run `conda install -c conda-forge cmake` inside your active environment.
- **WSL2 / Ubuntu / Linux**: Run `sudo apt install cmake`.
- **macOS**: Run `brew install cmake`.

Verify it works: `cmake --version` should return `3.20` or higher.

---

### 🛑 `Error: Executable build\granite_main.exe not found. Did you run 'build' first?`
**Reason**: The `run` command was invoked before a successful build, or the build failed silently.
**Solution**:
1. Run `python scripts/run_granite.py build` and read the full output carefully.
2. If you see any red errors, fix the dependency that is missing (see questions above).
3. After a **"Build successful!"** message, the `build/` folder will exist.
4. On Windows with MSVC, the exe may be in `build/Release/`. The runner handles this automatically.

---

### 🛑 `Error: 'cl' is not recognized` or `MSVC compiler not found`
**Reason**: Visual Studio Build Tools are not installed or not in PATH on Windows.
**Solution**: Install [Visual Studio Build Tools 2022](https://visualstudio.microsoft.com/visual-cpp-build-tools/) with the **"Desktop development with C++"** workload. Then reopen your terminal.

---

### 🛑 `CMake Error: CMake 3.20 or higher is required`
**Reason**: Your installed CMake is too old.
**Solution**: Upgrade CMake using the appropriate method for your environment:
- **Conda**: `conda install -c conda-forge cmake`
- **Ubuntu**: `sudo snap install cmake --classic` (gets the latest version)
- **macOS**: `brew upgrade cmake`

---

### 🛑 `Could NOT find HDF5` during CMake configuration
**Reason**: HDF5 development headers are not installed or not on the library search path.
**Solution by environment:**
- **Ubuntu/Debian**: `sudo apt install libhdf5-dev`
- **Fedora**: `sudo dnf install hdf5-devel`
- **Conda**: `conda install -c conda-forge hdf5`
- **macOS**: `brew install hdf5`

---

### 🛑 `Could NOT find MPI` during CMake configuration
**Reason**: OpenMPI development headers are not installed.
**Solution by environment:**
- **Ubuntu/Debian**: `sudo apt install libopenmpi-dev`
- **Fedora**: `sudo dnf install openmpi-devel && module load mpi/openmpi-x86_64`
- **Conda**: `conda install -c conda-forge openmpi`
- **macOS**: `brew install open-mpi`

---

### 🛑 `python: command not found`
**Reason**: On Linux/macOS, only `python3` is available by default. `python` may not exist.
**Solution**: Use `python3` instead of `python` on all Linux/macOS/WSL2 systems.

---

### 🛑 `Permission denied` when running the executable
**Reason**: The compiled binary does not have execute permissions (common on Linux).
**Solution**: Make the executable runnable:
```bash
chmod +x build/granite_main
```

---

### 🛑 `Nothing to clean.` when running `python scripts/run_granite.py clean`
**Explanation**: This is **not an error**. It means no `build/` directory exists — either because you never built, or a previous `clean` already removed it. It is safe to ignore.

---

> [!NOTE]
> If you encounter an error not listed here, please open an **Issue** on the GitHub repository and include:
> 1. Your operating system and terminal type.
> 2. The complete output of `python scripts/run_granite.py build`.
> 3. The output of `cmake --version` and `python --version`.
