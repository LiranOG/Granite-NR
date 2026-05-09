# GRANITE-NR v0.6.8 — Master Validation Protocol

**Status:** Active  
**Version:** v0.6.8 (Architecture & Stability Release)  
**Shell:** PowerShell 7+ (Windows)  
**Last updated:** 2026-05-09  
**Author:** LiranOG

> [!IMPORTANT]
> This protocol is the **single source of truth** for pre-release validation.
> Every command is **copy-pasteable PowerShell**. Unix/bash equivalents are
> provided in collapsed blocks where applicable. Do NOT run partial tiers —
> execute sequentially from Tier 0 through Tier 5.

---

## 🛠️ TIER 0 — Environment & Version Sync

**Goal:** Confirm the local checkout is at `v0.6.8` across all version-bearing files.

### 0.1 — Python & venv

```powershell
python --version                          # Expect: Python 3.10+
.venv\Scripts\Activate.ps1                # Activate venv (create first if needed: python -m venv .venv)
pip show granite-analysis | Select-String "Version"   # Expect: 0.6.8
```

### 0.2 — Version Parity Check

```powershell
Select-String -Pattern "0\.6\.8" -Path version.txt, CMakeLists.txt, pyproject.toml, CITATION.cff, python\granite_analysis\__init__.py, scripts\run_granite_hpc.py
```

**Expected:** At least one match per file — all showing `0.6.8`. Zero matches for `0.6.7.2`.

### 0.3 — Counter-check (must return empty)

```powershell
Select-String -Pattern "0\.6\.7\.2" -Path version.txt, CMakeLists.txt, pyproject.toml, CITATION.cff
```

**Expected:** No output. If any line appears, the version bump is incomplete.

### 0.4 — Git State

```powershell
git log -1 --oneline                      # Verify latest commit is the version bump
git status --short                        # Should be clean (no unstaged changes)
```

| Check | Pass Criteria |
|-------|--------------|
| `version.txt` | Contains exactly `0.6.8` |
| `CMakeLists.txt` | `VERSION 0.6.8` |
| `pyproject.toml` | `version = "0.6.8"` |
| `CITATION.cff` | `version: 0.6.8` |
| `__init__.py` | `__version__ = "0.6.8"` |
| `run_granite_hpc.py` | `Version : 0.6.8` |
| Counter-check | Zero matches for `0.6.7.2` |

**TIER 0 VERDICT:** ✅ Pass if all 6 files show `0.6.8` and counter-check is empty.

---

## 🛡️ TIER 1 — Static Integrity Gates

**Goal:** Verify repository hygiene — no dead links, no schema drift, no stale paths.

### 1.1 — Health Check

```powershell
python scripts\health_check.py
```

**Expected:** Green output confirming build directory, compiler, and OpenMP status.

### 1.2 — Dead Link Audit

> [!NOTE]
> `audit_links.py` and `audit_params_schema.py` are planned for v0.7.
> Until they ship, use the manual equivalent below.

```powershell
# Manual: verify no broken relative links in docs/
Select-String -Pattern "\]\(\./" -Path docs\*.md -Recurse | ForEach-Object {
    $link = $_.Line -replace '.*\]\(\.\/', '' -replace '\).*', '' -replace '#.*', ''
    $target = Join-Path "docs" $link
    if (-not (Test-Path $target)) { Write-Output "BROKEN: $($_.Filename):$($_.LineNumber) -> $link" }
}
```

**Expected:** No `BROKEN:` output.

### 1.3 — YAML Schema Spot-Check

```powershell
# Verify all benchmark YAML files parse without error
python -c "import yaml; [yaml.safe_load(open(f)) for f in __import__('glob').glob('benchmarks/*/params.yaml')]"
```

**Expected:** No exceptions.

### 1.4 — Source of Truth Alignment

```powershell
Select-String -Pattern "v0\.6\.8" -Path docs\SOURCE_OF_TRUTH.md
Select-String -Pattern "v0\.6\.8" -Path docs\VALIDATION_STATUS.md
```

**Expected:** Both files reference `v0.6.8`.

**TIER 1 VERDICT:** ✅ Pass if health check green, zero broken links, YAML parses, and docs aligned.

---

## 🏗️ TIER 2 — Build System Stress Test

**Goal:** Full clean rebuild with Release optimizations. Confirm the build produces a working binary.

### 2.1 — Clean Build

```powershell
# Nuke previous build artifacts
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue

# Full Release build
python scripts\run_granite.py build --build-type Release
```

### 2.2 — Binary Existence Check

```powershell
Test-Path build\bin\granite_main.exe       # Windows: expect True
Test-Path build\bin\granite_tests.exe      # Windows: expect True
# WSL/Linux: build/bin/granite_main and build/bin/granite_tests (no .exe)
```

### 2.3 — Version Banner Verification

```powershell
# If the binary can be invoked (WSL/Linux):
# ./build/bin/granite_main --version 2>&1 | Select-String "0.6.8"
#
# On Windows (no MPI): verify the GRANITE_VERSION_STR macro was injected:
Select-String -Pattern "GRANITE_VERSION_STR" -Path CMakeLists.txt
```

**Expected:** The CMake definition `GRANITE_VERSION_STR` is present and set from `PROJECT_VERSION`.

> [!WARNING]
> **Known Windows limitation:** The CMake configure step requires `yaml-cpp`
> and `HDF5` system packages. On Windows without vcpkg/Conda, the
> `FetchContent` fallback for `yaml-cpp` may fail offline. This is a
> **documented v0.7 target** (offline build support). The protocol proceeds
> on the assumption that either (a) dependencies are installed, or (b) the
> build is validated under WSL2.

**TIER 2 VERDICT:** ✅ Pass if build completes, both binaries exist, and version macro is defined.

---

## 🧪 TIER 3 — Python Test Suite

**Goal:** All 8 parser tests pass with zero failures.

### 3.1 — Run Tests

```powershell
$env:PYTHONPATH = "python"
python -m pytest tests\python -q
```

### 3.2 — Expected Output

```
8 passed in X.XXs
```

### 3.3 — Strict Verification

```powershell
$env:PYTHONPATH = "python"
$result = python -m pytest tests\python -q 2>&1
if ($result -match "8 passed") {
    Write-Output "TIER 3: PASS — 8/8 Python tests passed"
} else {
    Write-Output "TIER 3: FAIL"
    Write-Output $result
}
```

**TIER 3 VERDICT:** ✅ Pass if exactly `8 passed` with 0 failures, 0 errors.

---

## 🌌 TIER 4 — Physics Sanity Runs

**Goal:** Confirm the engine produces physically correct output on two canonical benchmarks.

> [!NOTE]
> These benchmarks require a working C++ binary (`granite_main`). On Windows
> without MPI/HDF5, run under WSL2. The log-parsing commands below work in
> PowerShell on the resulting `run.log` file regardless of where the
> simulation was executed.

### 4.1 — Single Puncture (Schwarzschild Stability)

```powershell
# Run (WSL2 or Linux):
python scripts\run_granite.py run --benchmark single_puncture

# Or direct invocation:
# ./build/bin/granite_main benchmarks/single_puncture/params.yaml 2>&1 | Tee-Object -FilePath run_sp.log
```

**Log Parsing (PowerShell):**

```powershell
# Lapse collapse & recovery — alpha_center should drop toward ~0.03, then recover toward ~0.3
Select-String -Pattern "alpha_center" run_sp.log | Select-Object -Last 5

# Hamiltonian constraint — must be < 1e-3 and monotonically decreasing
Select-String -Pattern "\|\|H\|\|" run_sp.log | Select-Object -Last 5

# NaN events — must be zero
Select-String -Pattern "NaN" run_sp.log
```

| Metric | Pass Criteria |
|--------|--------------|
| `alpha_center` (final) | 0.02 – 0.10 (trumpet lapse) |
| `‖H‖₂` (final) | < 1e-3, monotonically decreasing |
| NaN events | 0 |

### 4.2 — Binary Black Hole Inspiral (B2_eq)

```powershell
python scripts\run_granite.py run --benchmark B2_eq
# Or: ./build/bin/granite_main benchmarks/B2_eq/params.yaml 2>&1 | Tee-Object -FilePath run_b2.log
```

**Log Parsing (PowerShell):**

```powershell
# Constraint decay
Select-String -Pattern "\|\|H\|\|" run_b2.log | Select-Object -Last 5

# Lapse diagnostic
Select-String -Pattern "alpha_center" run_b2.log | Select-Object -Last 5

# NaN safety
Select-String -Pattern "NaN" run_b2.log
```

| Metric | Pass Criteria |
|--------|--------------|
| `‖H‖₂` (final, t=500M) | < 1e-4, reduction factor > 50× |
| `alpha_center` | Recovers from collapse (> 0.02) |
| NaN events | 0 |
| Wall time (64³, i5-8400) | ~99 min ± 20% |

**TIER 4 VERDICT:** ✅ Pass if both benchmarks complete NaN-free with constraints within tolerances.

---

## 🏛️ TIER 5 — Architectural Sanity

**Goal:** Confirm the v0.6.8 modular decomposition is intact — the monolith is dead.

### 5.1 — Monolith is Dead

```powershell
$mainLines = (Get-Content src\main.cpp).Length
Write-Output "src/main.cpp: $mainLines lines"
if ($mainLines -lt 100) {
    Write-Output "PASS — main.cpp is a thin wrapper ($mainLines lines)"
} else {
    Write-Output "FAIL — main.cpp is still a monolith ($mainLines lines)"
}
```

**Expected:** < 100 lines (current: 68 lines). `main.cpp` must contain ONLY:
- `#include` directives
- `PetscInitialize` / `MPI_Init` guards
- `parseConfig()` → `setupSimulation()` → `runEvolutionLoop()`
- `PetscFinalize` / `MPI_Finalize` teardown (correct order: PETSc before MPI)

### 5.2 — Modular Component Existence

```powershell
@(
    "src\core\cli_parser.cpp",
    "src\core\simulation_setup.cpp",
    "src\core\evolution_loop.cpp"
) | ForEach-Object {
    $exists = Test-Path $_
    $lines = if ($exists) { (Get-Content $_).Length } else { 0 }
    Write-Output "$_  exists=$exists  lines=$lines"
}
```

**Expected:**
| Component | Expected Lines | Responsibility |
|-----------|:-:|---|
| `cli_parser.cpp` | ~150 | YAML parsing, `printBanner()`, `parseConfig()` |
| `simulation_setup.cpp` | ~330 | `setupSimulation()`, `syncBlocks()`, AMR init |
| `evolution_loop.cpp` | ~720 | RK3 time loop, ghost exchange, Sommerfeld BC |

### 5.3 — Teardown Order (Critical)

```powershell
# PetscFinalize MUST appear BEFORE MPI_Finalize
$content = Get-Content src\main.cpp -Raw
$petscPos = $content.IndexOf("PetscFinalize")
$mpiPos   = $content.IndexOf("MPI_Finalize")
if ($petscPos -lt $mpiPos) {
    Write-Output "PASS — PetscFinalize() before MPI_Finalize()"
} else {
    Write-Output "FAIL — CRITICAL: MPI_Finalize before PetscFinalize (crash risk)"
}
```

### 5.4 — Rule-of-Five (BlockBundle)

```powershell
Select-String -Pattern "= default|= delete" -Path include\granite\simulation_setup.hpp
```

**Expected:** Lines showing defaulted move constructor/assignment and deleted copy constructor/assignment.

### 5.5 — `-ffast-math` Removal Confirmation

```powershell
Select-String -Pattern "ffast-math" -Path CMakeLists.txt
```

**Expected:** **No output.** If `-ffast-math` appears, the Sprint 1 fix has regressed.

### 5.6 — CMake Scoping (No Global Includes)

```powershell
Select-String -Pattern "^include_directories\(" -Path CMakeLists.txt
```

**Expected:** **No output.** All include paths must use `target_include_directories()`.

**TIER 5 VERDICT:** ✅ Pass if main.cpp < 100 lines, all 3 modules exist, teardown order correct,
Rule-of-Five present, no `-ffast-math`, and no global `include_directories()`.

---

## 📋 FINAL VERDICT MATRIX

| Tier | Gate | Status |
|------|------|:------:|
| T0 | Version sync (6 files at 0.6.8) | ⬜ |
| T1 | Static integrity (health check, links, YAML) | ⬜ |
| T2 | Clean Release build + binary existence | ⬜ |
| T3 | Python tests (8/8 passed) | ⬜ |
| T4 | Physics sanity (SP + B2_eq, NaN-free) | ⬜ |
| T5 | Architecture (monolith dead, teardown, CMake) | ⬜ |

**Release gate:** All 6 tiers must show ✅ before tagging `v0.6.8` and merging to `main`.

> Mark each tier ✅ as you complete it. If any tier fails, **STOP** — do not
> proceed to the next tier. Fix the failure first, then restart from Tier 0.

---

*GRANITE-NR v0.6.8 — Master Validation Protocol — May 9, 2026*
