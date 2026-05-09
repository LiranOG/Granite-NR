# Universal Deployment & Performance Optimization

This guide outlines the professional "Maximum Power" protocols for deploying the GRANITE numerical relativity engine. The engine uses massive dense grid computations natively, making compiler optimization thresholds and thread saturation mission-critical.

---

## 1. The "Maximum Power" Build Protocol

GRANITE enforces stringent Release flags on all compilers. If you build using the standard CLI instructions, `CMakeLists.txt` automatically triggers the following flags:

### 🪟 Windows Native (MSVC / cl.exe)
If compiling via Visual Studio Build Tools, the build system forces:
- `/O2`: Aggressive overarching optimization.
- `/Ob2`: Maximum function inlining (drastically improves CCZ4 inner loop speed).
- `/arch:AVX2`: Forces the compiler to emit Advanced Vector Extensions instructions natively supported by modern CPUs.
- `/fp:fast`: Overrides standard IEEE 754 precision to allow algebraic simplifications in heavily-nested floating point code, massively boosting speed.

**Native MSVC Checkpoint:** Ensure you are running CMake from a "Developer Command Prompt for VS 2022" or passing the correct architecture flag to `vcvars64.bat`. 

### 🐧 Linux / macOS (GCC / Apple Clang)
If compiling via WSL2, Ubuntu, Fedora, or Homebrew, the build system forces:
- `-O3`: Aggressive loop vectorization and optimization.
- `-march=native`: Extremely powerful flag that tunes GCC directly to the physical silicon architecture running the build (e.g. AVX-512). *Warning: The resulting binary cannot be shared directly with an older machine!*
- `-ffast-math`: Disables strict NaN propagation and algebraic safety to maximize FMA (Fused Multiply Accumulate) pipelines.

---

## 2. Hardware Validation (OpenMP Thread Saturation)

By default, GRANITE utilizes OpenMP to parallelize the RK3 time integration and CCZ4 spatial derivatives across all available physical and logical cores.

### How to check thread saturation:

1. Look closely at the terminal output when running a simulation.
2. The initial log will output: `[OpenMP] Allocated N threads for AMR subcycling.`
3. If `N` does not match your total CPU cores (`os.cpu_count()`), you have a saturation failure.

### Resolving Saturation Bottlenecks

OpenMP limits are heavily influenced by your session environment variables. To force saturation, execute the following before running the Python script:

**Linux / WSL2 / macOS (Bash):**
```bash
export OMP_NUM_THREADS=$(nproc)
export OMP_PROC_BIND=true
export OMP_PLACES=cores
```

**Windows (PowerShell):**
```powershell
$env:OMP_NUM_THREADS = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors
$env:OMP_PROC_BIND = "true"
$env:OMP_PLACES = "cores"
```

*Note: `OMP_PROC_BIND=true` prevents the OS from migrating threads between cores, which drastically improves cache coherency on L3/L2 caches when processing massive `GridBlock` vectors.*

---

## 3. Diagnostic Health Check 

If your simulation is running slow, immediately run the included diagnostic tool:

```bash
python scripts/health_check.py
```

This Python script performs a post-build forensic analysis:
1. **Cache Inspection:** It reads `build/CMakeCache.txt`.
2. **Build Type:** It alerts you if `-DCMAKE_BUILD_TYPE=Debug` sneaked into your build (which results in a 40x speed penalty).
3. **OpenMP Linkage:** It validates whether `find_package(OpenMP ...)` successfully routed to your compiler's threaded backend.
4. **Hardware Validation:** It cross-references your physical logical cores with the OpenMP environmental allocations.

---

## 4. Cross-Platform Execution Cheat-Sheet

Once correctly built, simulate anywhere exactly the same way:

| Goal | Command (Bash — Linux/Mac) | Command (PowerShell — Windows) |
| --- | --- | --- |
| **Integrated Dev Pipeline** | `python3 scripts/run_granite.py dev` | `python scripts/run_granite.py dev` |
| **Live Dashboard (stdin)** | `python3 -m granite_analysis.cli.dev_benchmark` | `python -m granite_analysis.cli.dev_benchmark` |
| **Sim Tracker (log file)** | `python3 -m granite_analysis.cli.sim_tracker run.log` | `python -m granite_analysis.cli.sim_tracker run.log` |
| **Export Telemetry (JSON)** | `python3 -m granite_analysis.cli.dev_benchmark --json out.json` | `python -m granite_analysis.cli.dev_benchmark --json out.json` |
| **Watch Mode (auto-rebuild)** | `python3 scripts/run_granite.py dev --watch` | `python scripts/run_granite.py dev --watch` |
| **Clean Output** | `python3 scripts/run_granite.py clean` | `python scripts/run_granite.py clean` |

---

## 5. Performance Tuning: Threads

### The Physical vs. Logical Core Bottleneck in GRMHD / CCZ4 Simulations

Modern CPUs expose two thread counts to the operating system:

| Concept | What the OS sees | Reality |
|---|---|---|
| **Physical Cores** | True independent execution units with dedicated L1/L2 caches and ALUs | Each core computes independently in parallel |
| **Logical Cores** (HT/SMT) | Two virtual threads scheduled on the *same* physical core | They share the same ALU, FP unit, and L1/L2 cache |

For **general-purpose workloads** (web servers, IDEs, compilation), Hyper-Threading is a net win: one thread stalls on I/O while the other uses the idle units.

For **GRANITE's CCZ4 spatial derivative stencils and RK3 time integration**, Hyper-Threading is a **performance liability**:

1. **Cache Thrashing:** GRANITE's `GridBlock` objects are large (often > 256 KB per tile). Two HT threads on the same core **compete for the same L1/L2 cache**, causing constant evictions and cache-miss penalties.
2. **FP Contention:** The CCZ4 inner loop is dominated by floating-point multiply-accumulate (FMA). On a hyper-threaded core pair, both threads share a **single SIMD FP pipeline** — meaning you get zero throughput gain from the second logical thread.
3. **False Sharing:** If two logical threads write to adjacent memory locations that happen to share a cache line (common in `double` arrays), the hardware must perform expensive cache-line invalidation protocols across cores.

> **Rule of thumb:** For GRANITE GRMHD/CCZ4 workloads, set `OMP_NUM_THREADS` to **physical core count**, not logical core count.

---

### Recommended Thread Counts for Common High-End CPUs

| Processor | Physical Cores | Logical Cores (HT) | Recommended `OMP_NUM_THREADS` |
|---|:---:|:---:|:---:|
| AMD Threadripper PRO 5995WX | 64 | 128 | **64** |
| AMD Threadripper PRO 5965WX | 24 | 48 | **24** |
| AMD Threadripper 3990X | 64 | 128 | **64** |
| AMD Threadripper 3960X | 24 | 48 | **24** |
| AMD Ryzen 9 7950X | 16 | 32 | **16** |
| AMD Ryzen 9 7900X | 12 | 24 | **12** |
| AMD Ryzen 9 5950X | 16 | 32 | **16** |
| AMD Ryzen 9 5900X | 12 | 24 | **12** |
| Intel Core i9-14900K | 8P+16E=24 | 32 | **24** (all P+E) |
| Intel Core i9-13900K | 8P+16E=24 | 32 | **24** (all P+E) |
| Intel Core i9-12900K | 8P+8E=16 | 24 | **16** (all P+E) |
| Apple M3 Max (16-core) | 16 | 16 | **16** (no HT) |
| Apple M2 Ultra (24-core) | 24 | 24 | **24** (no HT) |

> **Intel Hybrid (P+E) Note:** Intel Core 12th/13th/14th gen feature both Performance cores (P-cores, with HT) and
> Efficiency cores (E-cores, no HT). For GRANITE workloads, using `OMP_NUM_THREADS = P-cores + E-cores` (not
> counting HT virtual threads) is recommended. OpenMP's thread scheduler will naturally favour the P-cores for
> compute-heavy work while E-cores handle lighter duty.

---

### How GRANITE Auto-Tunes Threads

As of **v0.6.8**, the `run_granite.py` wrapper automatically detects and injects the correct `OMP_NUM_THREADS`
at process launch — you no longer need to set this manually for typical use:

```
[HPC] Auto-setting OMP_NUM_THREADS to 16 (Physical Core Count) for peak performance.
[HPC] Method: psutil  |  OMP_PROC_BIND=true  |  OMP_PLACES=cores
```

To override this behaviour and force a specific count (e.g., for benchmarking or shared-system courtesy):

```bash
# Linux / macOS / WSL2
export OMP_NUM_THREADS=8

# Windows PowerShell
$env:OMP_NUM_THREADS = 8
```

The diagnostic tool will verify your configuration and print shell-ready copy-paste commands if anything
is misconfigured:

```bash
python scripts/health_check.py
```

---

*GRANITE v0.6.8 — Deployment & Performance Optimization — April 2026*
