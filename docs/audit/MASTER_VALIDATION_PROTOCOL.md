# GRANITE-NR — Master Validation Protocol
**Release:** v0.6.7.2 · **Branch:** `audit/v0.6.7.2-doc-sync`
**Estimated Wall-Clock Time:** 45–60 minutes (solo developer)

> [!IMPORTANT]
> This checklist is the **single authoritative gate** between the `audit/v0.6.7.2-doc-sync`
> branch and a merge to `main`. Every box must be checked or annotated with an
> "Expected failure" justification before the merge is authorized.

---

## 🛠️ TIER 0: PRE-FLIGHT & ENVIRONMENT

- [ ] **Python version** — confirm 3.11+:
  ```bash
  python3 --version
  ```
  Verify: output shows `Python 3.11.x` or later.

- [ ] **Virtual environment active** (if used):
  ```bash
  source .venv/bin/activate   # Linux/macOS
  .\.venv\Scripts\Activate    # Windows PowerShell
  ```

- [ ] **Core Python deps available**:
  ```bash
  python3 -c "import yaml; import pytest; print('deps OK')"
  ```
  Verify: prints `deps OK`.

- [ ] **CMake available** (Linux/macOS only):
  ```bash
  cmake --version
  ```
  Verify: `cmake version 3.20` or later. *(Expected failure on Windows dev
  machines without CMake in PATH — log and continue.)*

- [ ] **Version string sync** — all three sources must agree:
  ```bash
  grep -n "0.6.7.2" version.txt CMakeLists.txt src/core/cli_parser.cpp
  ```
  Verify: exactly 3 matches:
  | File | Line | Content |
  |------|------|---------|
  | `version.txt` | 1 | `0.6.7.2` |
  | `CMakeLists.txt` | 10 | `VERSION 0.6.7.2` |
  | `src/core/cli_parser.cpp` | 22 | `"0.6.7.2"` |

---

## 🛡️ TIER 1: STATIC INTEGRITY GATES

- [ ] **Link audit** — no stale Granite links:
  ```bash
  python3 scripts/audit_links.py
  ```
  Verify: exit code 0. Output contains `Link audit passed` or zero `[FAIL]` lines.

- [ ] **Schema audit** — benchmark YAML keys valid:
  ```bash
  python3 scripts/audit_params_schema.py
  ```
  Verify: zero lines matching `[FAIL]`. All runnable benchmarks show `[PASS]`.

- [ ] **Formatting check** — `clang-format` dry run:
  ```bash
  find src/ include/ -name '*.cpp' -o -name '*.hpp' | \
    xargs clang-format-18 --dry-run --Werror 2>&1 | head -20
  ```
  Verify: no output = all files compliant. *(If `clang-format-18` is not
  installed, log as non-blocking and continue.)*

- [ ] **Stale link grep** — final sweep:
  ```bash
  grep -Rn "github\.com/LiranOG/Granite[^-N]" \
    README.md docs .github benchmarks scripts viz \
    CHANGELOG.md CITATION.cff pyproject.toml CMakeLists.txt 2>/dev/null
  ```
  Verify: **zero matches**.

---

## 🏗️ TIER 2: BUILD SYSTEM VALIDATION

- [ ] **Clean build directory**:
  ```bash
  rm -rf build/
  ```

- [ ] **CMake configure**:
  ```bash
  python3 scripts/run_granite.py build --build-type Release 2>&1 | tee build.log
  ```
  Verify in `build.log`:
  - `-- Configuring done` appears.
  - `-- Generating done` appears.
  - `GRANITE Build Configuration  v0.6.7.2` banner is printed.

  > **Expected failure (Windows):** HDF5 `find_package` will fail if `libhdf5`
  > is not installed. This produces `FATAL_ERROR` on the HDF5 block. To
  > continue, re-run with HDF5 disabled:
  > ```bash
  > cmake -B build -DCMAKE_BUILD_TYPE=Release -DGRANITE_ENABLE_HDF5=OFF
  > ```
  > Confirm `Configuring done` with HDF5 disabled.

- [ ] **Verify no GLOB_RECURSE in build system**:
  ```bash
  grep -c "GLOB_RECURSE" CMakeLists.txt
  ```
  Verify: `0` (comment-only references do not count as functional usage).

- [ ] **Object library targets present**:
  ```bash
  grep "add_library.*OBJECT" CMakeLists.txt
  ```
  Verify: 9 OBJECT library targets listed (`granite_core`, `granite_spacetime`,
  `granite_grmhd`, `granite_radiation`, `granite_amr`, `granite_initial_data`,
  `granite_horizon`, `granite_io`, `granite_postprocess`).

---

## 🧪 TIER 3: FUNCTIONAL REGRESSION SUITE

- [ ] **Python test suite**:
  ```bash
  PYTHONPATH=python pytest -q tests/python
  ```
  Verify: exactly **8 tests passed**, 0 failed, 0 errors.

  ```
  ........                                                           [100%]
  8 passed
  ```

- [ ] **Test baseline unchanged**:
  ```bash
  grep -c "def test_" tests/python/test_parsers.py
  ```
  Verify: count = 8 (matches the v0.6.7.2 baseline established in Mission 0).

---

## 🌌 TIER 4: PHYSICS SANITY RUNS

> [!NOTE]
> These runs require a successful C++ build (`granite_main` binary).
> If the build is blocked by missing system dependencies (HDF5, MPI),
> mark these as **DEFERRED** and note the blocking dependency.

### SIMULATION A — Single Puncture (Schwarzschild)

- [ ] **Run**:
  ```bash
  ./build/bin/granite_main benchmarks/single_puncture/params.yaml 2>&1 | tee sp_run.log
  ```
  Run for at least T = 10M (set `t_final: 10.0` in params if needed).

- [ ] **Lapse collapse**:
  ```bash
  grep "alpha_center" sp_run.log | tail -5
  ```
  Verify: `alpha_center` drops from ~1.0 toward **~0.3** (trumpet solution).
  Tolerance: any value < 0.5 at t ≥ 5M confirms the puncture is evolving.

- [ ] **Hamiltonian constraint**:
  ```bash
  grep "||H||" sp_run.log | tail -5
  ```
  Verify: `||H||₂ < 1e-3` throughout the evolution.

- [ ] **NaN freedom**:
  ```bash
  grep -c "NaN" sp_run.log
  ```
  Verify: **0** (zero NaN instances).

### SIMULATION B — Gauge Wave

- [ ] **Run**:
  ```bash
  ./build/bin/granite_main benchmarks/gauge_wave/params.yaml 2>&1 | tee gw_run.log
  ```
  Run for 1 full period (default params are preconfigured).

- [ ] **Hamiltonian stability**:
  ```bash
  grep "||H||" gw_run.log | tail -5
  ```
  Verify: `||H||₂` remains < **1e-6** (flat-spacetime gauge wave should
  produce near-zero constraint violation).

- [ ] **NaN freedom**:
  ```bash
  grep -c "NaN" gw_run.log
  ```
  Verify: **0**.

### SIMULATION C — B2_eq (BBH Early Inspiral)

- [ ] **Run**:
  ```bash
  ./build/bin/granite_main benchmarks/B2_eq/params.yaml 2>&1 | tee b2_run.log
  ```
  Run for at least T = 20M.

- [ ] **Constraint convergence**:
  ```bash
  grep "||H||" b2_run.log | tail -5
  ```
  Verify: `||H||₂ < 1e-2` (early inspiral; BBH constraints are larger than
  single puncture due to finite separation).

- [ ] **NaN freedom**:
  ```bash
  grep -c "NaN" b2_run.log
  ```
  Verify: **0** NaN instances in stdout.

- [ ] **CFL stability**:
  ```bash
  grep -c "CFL-GUARD" b2_run.log
  ```
  Verify: **0** (no CFL violations triggered during evolution).

---

## 📚 TIER 5: DOCUMENTATION & WIKI SYNC

- [ ] **Wiki diff files exist**:
  ```bash
  ls -la audit/wiki-*.diff
  ```
  Verify: at least 3 diff files present:
  - `wiki-home-changes.diff`
  - `wiki-param-ref-changes.diff`
  - `wiki-roadmap-changes.diff`
  - `wiki-history-alignment.diff`

- [ ] **Apply Wiki diffs** (manual — requires Wiki repo clone):
  ```bash
  cd ../Granite-NR.wiki
  git apply ../Granite-main420/audit/wiki-home-changes.diff
  git apply ../Granite-main420/audit/wiki-param-ref-changes.diff
  git apply ../Granite-main420/audit/wiki-roadmap-changes.diff
  git apply ../Granite-main420/audit/wiki-history-alignment.diff
  git diff --stat
  ```
  Review changes. Push only after manual inspection.

- [ ] **VALIDATION_STATUS.md milestones correct**:
  ```bash
  grep "v0.7" VALIDATION_STATUS.md
  ```
  Verify: all `v0.7` references point to modules, not dates. No "Q2 2026" in
  future-facing contexts.

- [ ] **Roadmap dates correct**:
  ```bash
  grep -E "Q[1-4] 202[67]" README.md | head -10
  ```
  Verify the "Solo-Dev Stability" schedule:
  | Version | Quarter |
  |---------|---------|
  | v0.6.5  | Q2 2026 |
  | v0.6.7  | Q2 2026 |
  | v0.7.0  | Q4 2026 |
  | v0.8.0  | Q1 2027 |
  | v0.9.0  | Q2 2027 |
  | v1.0.0  | Q3 2027 |

- [ ] **Issues tracker seeded**:
  ```bash
  cat audit/issues-to-open.md | grep "^## " | wc -l
  ```
  Verify: **8** issue headers present.

---

## 🏛️ TIER 6: ARCHITECTURAL SANITY

- [ ] **`main.cpp` is lean**:
  ```bash
  wc -l src/main.cpp
  ```
  Verify: **< 100 lines** (target: ~61 lines after decomposition).

- [ ] **No circular includes**:
  ```bash
  grep "#include" include/granite/evolution_loop.hpp
  grep "#include" include/granite/simulation_setup.hpp
  ```
  Verify: `evolution_loop.hpp` includes `simulation_setup.hpp` but NOT vice versa.
  `simulation_setup.hpp` includes `cli_parser.hpp` but NOT `evolution_loop.hpp`.

- [ ] **TimeIntegrator is internal** (no ODR violation):
  ```bash
  grep -c "class TimeIntegrator" include/granite/evolution_loop.hpp
  ```
  Verify: **0** (class defined only in `src/core/evolution_loop.cpp`).

- [ ] **BlockBundle::id initialized**:
  ```bash
  grep "int id" include/granite/simulation_setup.hpp
  ```
  Verify: shows `int id = -1;` (not bare `int id;`).

- [ ] **SPDX headers present on all source files**:
  ```bash
  find src/ include/ -name '*.cpp' -o -name '*.hpp' | \
    xargs head -1 | grep -c "SPDX-License-Identifier"
  ```
  Verify: count matches total number of `.cpp` + `.hpp` files.

---

## ✅ FINAL SIGN-OFF

- [ ] All TIER 0–6 boxes checked or annotated with "Expected failure" + reason.
- [ ] `git log --oneline main..HEAD` reviewed — commit history is clean.
- [ ] `git diff main..HEAD --stat` reviewed — no unexpected file changes.
- [ ] Run `git status` — working tree is clean.
- [ ] **DECISION:** Ready to merge `audit/v0.6.7.2-doc-sync` → `main`.
- [ ] Owner executes: `git checkout main && git merge --no-ff audit/v0.6.7.2-doc-sync`
- [ ] Owner executes: `git push origin main` (after personal review).

---

## Physics Threshold Reference

| Simulation | Metric | Pass Threshold | Source |
|---|---|---|---|
| Single Puncture | `alpha_center` at t ≥ 5M | < 0.5 (expect ~0.3) | Trumpet solution, Hannam+ 2007 |
| Single Puncture | `\|\|H\|\|₂` | < 1e-3 | NRCF convergence standard |
| Gauge Wave | `\|\|H\|\|₂` | < 1e-6 | Flat-space apples-with-apples |
| B2_eq | `\|\|H\|\|₂` | < 1e-2 | BBH early inspiral tolerance |
| All simulations | NaN count | = 0 | IEEE 754 cleanliness |
| B2_eq | CFL-GUARD triggers | = 0 | Numerical stability |

---

*Protocol created: 2026-05-08 · Author: GRANITE-NR Stabilization Pass*
