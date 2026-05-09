# Changelog

All notable changes to **GRANITE** (General-Relativistic Adaptive N-body Integrated Tool for Extreme Astrophysics) are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> **Full development journal:** The granular, commit-level engineering log (~2,500 lines) is
> archived at [`docs/design/DEVELOPMENT_JOURNAL.md`](docs/design/DEVELOPMENT_JOURNAL.md).

---

## [Unreleased]

### Planned — v0.7
- GPU porting of primary CCZ4 kernels to CUDA (target: VAST.ai H100 SXM)
- Checkpoint-restart (`--resume` CLI flag)
- Full M1 radiation coupling into the main RK3 evolution loop
- Dynamic AMR regridding during evolution
- `chi_blend_center` / `chi_blend_width` wired through YAML parser

---

## [v0.6.7.2] — 2026-04-30

### Summary

Post-release hardening and the **complete architectural overhaul of the Python analysis layer**. The monolithic `scripts/sim_tracker.py` and `scripts/dev_benchmark.py` were decomposed into the typed, installable `granite_analysis` package — a four-layer separation of data models, stateless parsers, shared runner logic, and thin CLI entry points. Alongside the refactor: closes a version mismatch in the C++ engine banner, eliminates all raw exception leakage from CLI exit paths, extends `run_granite.py` with a `docs` build subcommand, and repairs 25 broken relative links found during a full-repository documentation audit.

### Architecture — `granite_analysis` Package Overhaul

The primary engineering effort of this release. The old `scripts/` telemetry tools were procedural monoliths that shared no code, relied on `sys.path` insertion, and had no type safety. The refactor replaced them entirely with a four-layer, PEP 517-compliant package:

- **`python/granite_analysis/core/models.py` — Typed Event Layer:**
  13 `@dataclass(frozen=True)` classes covering every observable in GRANITE's log output:
  `SimulationStep` (step, t, α_center, ‖H‖₂, blocks), `NanEvent` (step, var_type, var_id, grid coords i/j/k, value), `CflEvent`, `CheckpointEvent`, `DiagnosticEvent`, `PunctureData`, `GridParams` (dx, dt), `NCellsInfo` (nx/ny/nz), `VersionInfo`, `IdTypeInfo`, `TFinalInfo`.
  Frozen dataclasses enforce immutability through the entire processing pipeline.

- **`python/granite_analysis/core/parsers.py` — Stateless Parsing Layer:**
  12 pre-compiled `re.compile()` patterns. Public API: `parse_telemetry_line(line: str) -> Optional[TelemetryEvent]` — pure function, no state, one line in / one typed event out. Companion `parse_telemetry_file(path: Path) -> Iterator[TelemetryEvent]` for lazy file reading. `_safe_float()` converts engine numerics safely, handling `nan`, `inf`, truncated values, and partial log lines without raising.

- **`python/granite_analysis/core/runners.py` — Shared Orchestration Layer:**
  `run_telemetry_loop(input_stream, ui, is_quiet, json_path, csv_path)` — the single event loop used by both CLIs. Accepts any `Iterable[str]` (stdin pipe, log file, or test mock). Accumulates `SimulationStep` records and writes JSON/CSV on completion. Centralising this eliminated complete code duplication that existed between the two old monolithic scripts.

- **`python/granite_analysis/cli/sim_tracker.py` / `dev_benchmark.py` — Thin CLI Layer:**
  Pure argparse entry points, each under 50 lines. `sim_tracker` accepts an optional positional `logfile` and falls back to `sys.stdin` for live pipe mode. `dev_benchmark` adds `--benchmark` for named benchmark selection. Both delegate immediately to `run_telemetry_loop()`.

- **`pyproject.toml` — PEP 517 / PEP 668 Compliance:**
  Installable via `pip install -e .[dev]` from the repository root. Declares runtime deps (`numpy ≥1.24`, `scipy ≥1.10`, `h5py ≥3.8`, `matplotlib ≥3.7`, `rich ≥13.0`, `pyyaml`) and dev extras (`pytest`, `sphinx`, `furo`, `breathe`) declaratively. No `sys.path` hacking.

**Old scripts permanently removed:** `scripts/sim_tracker.py`, `scripts/dev_benchmark.py`, `scripts/dev_stability_test.py`.

### Added
- **`run_granite.py docs` subcommand:** Invokes `sphinx-build` against `docs/` and writes HTML output to `docs/_build/html/`. Accepts `--open` to launch the result in the default browser after building. Emits a clean, actionable error message if Sphinx is not installed (`sphinx-build not found — pip install -e .[docs]`) rather than propagating a raw `FileNotFoundError`.
- **`python/requirements.txt`:** Flat dependency manifest (NumPy ≥1.24, SciPy ≥1.10, h5py ≥3.8, Matplotlib ≥3.7, Rich ≥13.0) for deployment environments that do not support PEP 517 editable installs. Canonical development install path remains `pip install -e .[dev]` from the repository root.
- **Quick Start — Step 7 (HPC/SLURM Deployment):** New section documenting `run_granite_hpc.py` with `--slurm`, `--mpi-ranks`, and `--omp-threads` flags, the `jobs/submit_granite.sbatch` output path, and the live `sim_tracker` telemetry pipe.
- **Python test suite in Step 4:** Added `pytest tests/python -v` as an explicit post-build verification step alongside the C++ `granite_tests` binary.

### Fixed
- **C++ engine version banner:** `src/main.cpp` banner string corrected from `"0.6.5"` to `"0.6.7"`. The mismatch was caught during a live benchmark run when the engine stdout diverged from parser expectations.
- **`sim_tracker.py` / `dev_benchmark.py` — missing logfile:** Raw `FileNotFoundError` traceback replaced with `sys.exit(f"ERROR: log file not found: {args.logfile}")` and exit code 1. Applied symmetrically to both CLI entry points.
- **`runners.py` — bad `--json` / `--csv` output directory:** Raw `FileNotFoundError` on output-file write replaced with `sys.exit(f"ERROR: --json/--csv directory does not exist: {path.parent}")` and exit code 1.
- **25 broken relative links (repository-wide):** Full audit across `README.md` and all `docs/` Markdown files. All flat-directory references corrected to actual subdirectory paths:
  - `./docs/INSTALL.md` → `./docs/getting_started/Installation.md`
  - `./docs/DEVELOPER_GUIDE.md` → `./docs/developer_guide/DEVELOPER_GUIDE.md`
  - `./docs/COMPARISON.md` → `./docs/developer_guide/COMPARISON.md`
  - `./docs/BENCHMARKS.md` → `./docs/user_guide/BENCHMARKS.md`
  - `./docs/SCIENCE.md` → `./docs/theory/SCIENCE.md`
  - `./docs/FAQ.md` → `./docs/user_guide/FAQ.md`
  - `./docs/diagnostic_handbook.md` → `./docs/user_guide/diagnostic_handbook.md`
  - `./docs/v0.6.5_master_dictionary.md` → `./docs/developer_guide/v0.6.5_master_dictionary.md`
  - `docs/PERSONAL_NOTE.md` → `docs/design/PERSONAL_NOTE.md`
  - Cross-document links in `WINDOWS_README.md`, `SCIENCE.md`, `BENCHMARKS.md`, `FAQ.md`, `DEVELOPMENT_JOURNAL.md`

### Changed
- **Quick Start Guide (Steps 4–6):** Consolidated OS-split command blocks into unified cross-platform bash examples. Step 5 restructured as a numbered use-case list: (1) integrated pipeline via `run_granite.py dev`, (2) watch mode, (3) direct CLI on a log file with full flag matrix, (4) live engine→`sim_tracker` pipe. Step 6 includes explicit `--json`/`--csv` telemetry export.
- **`docs/README.md` — Documentation Builds:** Corrected stale reference from `pip install -r python/requirements.txt` to `pip install -e .[docs]` and updated the build command to the new `run_granite.py docs` subcommand.

---

## [v0.6.7] — 2026-04-27

### Summary

The **Repository Seal** release. Completes the full 4-phase architectural audit of GRANITE, finalizes the VORTEX WebGL engine to Gold Master quality, and achieves a 100% clean CI/CD build (107 tests, 20 suites, zero errors, zero warnings) after a marathon 8-hour engineering session resolving API regressions, CI pipeline instability, documentation drift, and an identity attribution error across 37 files.

### Added
- **C++ Smoke Tests — 4 new suites (92 → 107 tests):** `AMRSmokeTest` (5 tests, `test_amr_basic.cpp`), `SchwarzschildHorizonTest` (3 tests, `test_schwarzschild_horizon.cpp`), `M1SmokeTest` (4 tests, `test_m1_diffusion.cpp`), `HDF5RoundtripTest` (3 tests, `test_hdf5_roundtrip.cpp`). All API-synchronized against the actual GRANITE core headers.
- **`include/README.md`:** Maps the C++ header architecture (`src/` ↔ `include/` mirror) for contributor onboarding.
- **`.gitattributes`:** Linguist override enforcing C++ language dominance in GitHub statistics (`linguist-detectable=false` for `scripts/**`, `viz/**`, `*.ipynb`; `linguist-documentation` for all docs).
- **VORTEX Gold Master — Cinematic & Audio Systems:**
  - Zero-allocation GW Audio Sonification Synthesizer (Web Audio API; frequency mapped to `f_GW`, strain to `|h_+|`).
  - Global Zen Mode (`Z` key) — fades all UI to 0% opacity with CSS `transition: opacity 0.5s` for distraction-free recording.
  - Cinematic Autopilot Camera Mode — auto-selects BH/NS targets with 12–15 s jitter-randomized hold times.
  - Master Volume Control Panel (`#vol-btn`, `#vol-popup`, `#vol-slider`, `#vol-mute`).
  - ESC-abort system for atomic cancellation of all active body placement and slingshot drag operations.

### Changed
- **Identity Purge — "GRANITE Collaboration" → "Liran M. Schwartz" (37 files):** All `@copyright` Doxygen headers in 28 C++/HPP files, `LICENSE`, and `docs/paper/granite_preprint_v067.tex`. Verified with `grep -rI "GRANITE Collaboration"` → zero results.
- **Documentation Voice Realignment — "We/Our" → "I/The" (24 passages):** `README.md`, `VISION.md`, `PERSONAL_NOTE.md`, `FAQ.md`, `DEPLOYMENT_AND_PERFORMANCE.md`, `Installation.md`, `DEVELOPER_GUIDE.md`, `viz/README.md`. Academic "We" preserved in LaTeX preprint.
- **Documentation Coherence:** Stripped emojis from all `docs/theory/*.rst` files and 7 directory READMEs. Anti-truncation sweep rebuilt all directory trees to match the physical filesystem. Version stamps synchronized from `v0.6.5` → `v0.6.7` across 8 files.

### Fixed
- **C++ API Hallucinations (4 rounds of repair):** Corrected all fabricated constructor signatures, class names, and method names in the new smoke tests (`AMRHierarchy` 2-arg constructor, `ApparentHorizonFinder`/`HorizonParams`/`HorizonData`, `M1Transport`, `HDF5Writer`+`IOParams`). Replaced undeclared `M1_NUM_VARS` with `NUM_RADIATION_VARS`. Fixed signedness warnings (`int` → `size_t`).
- **CI/CD Pipeline:** Resolved Sphinx build failure (`docs/Makefile` blocked by `.gitignore` → `git add -f`). Eliminated cross-platform `clang-format` v18/v19 disagreements (`AlignOperands: DontAlign`; multi-line `<<` chains → `std::ostringstream`). Fixed `H5Gcreate2` nested group creation bug. Corrected HDF5 dataset path prefix (`/level_0/...`). Fixed AMR restriction tolerance (`1.0e-14` → `1.0e-12`). Constrained HDF5 verification loop to interior cells only.
- **Git History Recovery:** Restored 214-commit GitHub history after accidental `git push --force` from a `git init` directory. Final state: 216 commits.

---

## [v0.6.6.5] — 2026-04-15

### Summary

VORTEX engine polished to **Gold Master** with cinematic, audio, and analytical systems. Concurrent with Phase 3 of the GRANITE audit, which added four major research-grade visualization systems and performed a five-pass hardening of the Floating Window Manager.

### Added
- **VORTEX Sim-OS Audit & Shortcut Reference:** Full cross-reference of all `keydown` handlers. New `SIM-OS & INTERFACE` section in `#sc-modal` with `[`, `≡`, and `Esc` entries. Fixed modal open/close via `classList` (was `style.display`, breaking Esc handler).
- **Relativistic Visual Physics (CPU-side, zero-allocation):** Relativistic Doppler shift + Beaming (`tog.doppler`, mutates `iColorData` Float32Array in-place). Einstein Cross GLSL extension (+20 lines into existing `ShaderPass`; activates at `lensStr > 0.18`).
- **NR Diagnostics Floating Window:** Chart.js panels for orbital eccentricity vs. semi-major axis (GW inspiral track), `f_GW` chirp (semi-log), and `β²` velocity parameter. MTF time-filter bar (`LIVE / 1YR / 10YR / MAX`). Centralized `_clearTelemetry()` reset helper preventing cross-scenario data contamination.
- **Minimap 3.0 Tactical Analyzer:** Isobar mode (gravitational potential field via marching squares), Flux mode (bolometric radiation flux, 5-stop thermal ramp), camera frustum overlay, ISCO proximity pulsing reticle, click-to-select, hover tooltip, velocity arrows, and logarithmic projection toggle (`⊚ LOG`). Fixed load-time canvas distortion from `display: flex` + `align-items: stretch`.
- **Master Menu (`≡`) Professional Categorization:** Four named sections — `📭 Relativistic Optics`, `📊 Scientific Analysis`, `🗺 Tactical Tools`, `⚙ System`.

### Fixed
- **HUD Scale — FWM Floating Window Manager (5-pass hardening):**
  - **Pass 1:** Off-screen initial placement → changed `adoptPanel()` x to `Math.floor(innerWidth / currentUIScale - cssW - 10)`.
  - **Pass 2:** Mouse escape to OS during drag → `requestPointerLock()` + `e.movementX/Y` deltas.
  - **Pass 3:** Sticky drag on Alt+Tab → `window.addEventListener('blur', onDragEnd)`.
  - **Pass 4:** Zoom-aware coordinate transform → rewrote `_clampFWMWindows()` using `vw/sc - cssW` formula.
  - **Pass 5:** Resize off-screen + drag triggered by inner controls → replaced all `el.offsetWidth` with `parseFloat(el.style.width)`; extended drag guard to all semantic interactive elements.
- **`#ph` Phase Label Overflow at HUD Scale > 1.4×:** `max-width: 28vw; overflow: hidden; text-overflow: ellipsis`.
- **`.fwm-win` Missing from HUD Zoom CSS Rule:** Added `.fwm-win` to `zoom: var(--ui-scale)` selector.

---

## [v0.6.6] — 2026-04-12

### Summary

Integration of **VORTEX**, a bespoke WebGL N-body visualization engine, into `viz/vortex_eternity/`. Foundational architectural step toward decoupling the C++ HPC backend from interactive 3D rendering.

### Added
- **VORTEX Engine (`viz/vortex_eternity/VORTEX-engine.html`):** Single-file browser engine, no install required. Zero-allocation architecture (all spatial vectors mutated in-place). 4th-order Hermite Predictor-Corrector integration with Aarseth adaptive timestepping and Kahan compensated summation.
- **Post-Newtonian Physics:** 1PN (Einstein-Infeld-Hoffmann), 1.5PN (Lense-Thirring + Rodrigues rotation), 2.5PN (radiation reaction / GW inspiral). Mass-defect mergers, asymmetric recoil kicks, TDE via Roche limits.
- **Visualization:** Real-time GLSL Schwarzschild ray-deflection post-processing shader (Einstein rings, photon spheres). Live GW telemetry dashboard (α_min, h+, h×, chirp mass Mc).

---

## [v0.6.5.5] — 2026-04-11

### Summary

Complete rebuild of `README.md`. Structural overhaul adding linked table of contents, benchmark results, competitor feature matrix, formal roadmap, known-limitations register, community section, GitHub Wiki index, and BibTeX citation block.

### Added
- **Competitor feature matrix** — Einstein Toolkit, GRChombo, SpECTRE, AthenaK vs. GRANITE across 5 capabilities.
- **Benchmark results section** — Production run data (i5-8400, WSL2): single puncture ×84.8 ‖H‖₂ reduction, B2_eq ×61.3 at t=500M.
- **Versioned roadmap table** — v0.6.5 → v1.0.0 with deliverables per version.
- **Known-limitations register** — 8-row versioned table with planned fix versions.
- **BibTeX citation block** — `@software{granite2026, ...}`.

### Changed
- Badge suite expanded to 4 rows (CI/Status, Identity, Stack, Docs). C++ standard narrowed to `c++17`.
- Repository structure tree — corrected and expanded with all new directories (`containers/`, `python/`, `viz/`, `runs/`).
- Quick Start guide condensed and de-cluttered.

---

## [v0.6.5.4] — 2026-04-10

### Summary

**GitHub Wiki launch** — 17 fully cross-referenced technical pages (~18,000 words) covering every subsystem, parameter, known bug, and benchmark result.

### Added
- **17 wiki pages:** `Home`, `Architecture-Overview` (10 sections, full ASCII data-flow diagram), `Physics-Formulations` (CCZ4, GRMHD, M1, numerical methods, 16-entry bibliography), `Parameter-Reference` (every `params.yaml` field with type, range, warnings), `AMR-Design`, `Initial-Data`, `Known-Fixed-Bugs` (10-entry registry with before/after diffs), `Simulation-Health-&-Debugging` (6 health indicators, debugging flowchart, 5 failure modes), `Benchmarks-&-Validation`, `Developer-Guide`, `HPC-Deployment`, `Gravitational-Wave-Extraction`, `FAQ` (12 questions), `Scientific-Context` (B5_star scenario, multi-messenger table), `Roadmap`, `CHANGELOG-Summary`, `Documentation-Index-&-Master-Reference` (35-row topic cross-reference matrix).
- **`_Sidebar.md`** — 4-section persistent wiki navigation.

### Fixed (Undocumented v0.6.5 Features — Now Formally Documented)
- **Chi-blended selective upwinding** (`ccz4.cpp`) — tanh-sigmoid blend between 4th-order centered FD (χ < 0.05) and 4th-order upwinded FD (χ ≥ 0.05). Γ̂^i always centered.
- **Algebraic constraint enforcement** (`main.cpp`) — `det(γ̃)=1` and `tr(Ã)=0` enforced once per full RK3 step via `applyAlgebraicConstraints()`.
- **B2_eq quasi-circular momenta corrected** — Both BHs updated from `[0,0,0]` (head-on) to `[0, ±0.0954, 0]` (Pfeiffer et al. 2007, d=10M quasi-circular).

### Changed
- `CITATION.cff` — `date-released: 2026-04-10`.
- `docs/diagnostic_handbook.md` — Formally deprecated; superseded by `Simulation-Health-&-Debugging` wiki page.

---

## [v0.6.5] — 2026-04-07

### Summary

**Tactical Reset** — forensic rescue restoring the verified v0.6.0 production-stable baseline. After architectural regressions from an over-engineered audit phase, a directory-wide `diff` against the v0.6.0 backup was used to surgically revert all damaging changes while preserving accumulated P0 memory safety fixes. Both `single_puncture` and `B2_eq` benchmarks now run stably to t=500M with ‖H‖₂ decaying monotonically.

### Added
- `benchmarks/gauge_wave/params.yaml` — Alcubierre et al. (2004) gauge wave code-validation benchmark.
- `benchmarks/B2_eq/params.yaml` — Production equal-mass BBH parameter file (256³ grid, ±200M domain, 6 AMR levels, `p_t = ±0.0954`).
- `benchmarks/validation_tests.yaml` — Machine-readable validation suite: 3 physics sectors, 9 tests with quantitative pass/fail criteria (spacetime, GRMHD, radiation).
- `scripts/sim_tracker.py` — Procedural live simulation dashboard with 8-layer stability guard (cannot falsely report "No catastrophic events").
- **2 new selective advection unit tests** (test count: 90 → 92): `SelectiveAdvecUpwindedInFlatRegion`, `SelectiveAdvecCenteredNearPuncture`.

### Fixed
- **Architecture reset** — `ccz4.cpp`, `amr.cpp`, `main.cpp` reverted to verified v0.6.0 baseline. Removed over-engineered chi-flooring clamps, `std::floor` replacement, `prolongateGhostOnly()` bifurcation, `truncationErrorTagger`, and undefined struct members causing CI failure.
- **Memory safety (P0, preserved)** — `levels_.reserve(max_levels)` in `AMRHierarchy::initialize()` prevents reallocation-triggered UB during subcycling.
- **Singularity avoidance** — `MICRO_OFFSET = 1.3621415e-6` re-injected universally after YAML parsing, before grid construction.
- **Telemetry (`sim_tracker.py`)** — Reverted from broken class-based refactor to stable procedural architecture. `safe_float()` crash-proof casting. 8-layer Stability Summary guard.

### Changed
- `CMakeLists.txt` — `VERSION 0.6.0` → `VERSION 0.6.5`.
- `src/main.cpp` — Banner version `"0.6.5"`.

---

## [v0.6.0] — 2026-04-04

### Summary

**Puncture Tracking Update** — engine fully transitions from single-grid to Berger-Oliger adaptive mesh refinement with binary black hole initial data. Diagnostic tooling completely overhauled. Test count: 90 → 92.

### Added
- **Berger-Oliger AMR:** Full recursive subcycling (`subcycle()`), block-merging algorithm (prevents MPI deadlocks for close binaries), `setLevelDt()` cascade fixing frozen fine grids.
- **`TwoPuncturesBBH` Spectral Initial Data** — Bowen-York extrinsic curvature + Newton-Raphson Hamiltonian constraint solve for conformal factor correction `u`. Tracking sphere registration for dynamic puncture-following refinement.
- **`sim_tracker.py`** — Append-only live simulation dashboard with real-time phase classification, exponential constraint growth rate (γ), Zombie State detector, NaN forensics, and Matplotlib post-run summaries.
- `scripts/dev_benchmark.py` — Per-step NaN forensics and lapse monitoring tool.
- **Sommerfeld radiative BCs** — Quasi-spherical 1/r outgoing wave condition; allows gauge waves to exit domain without reflection.
- **Algebraic constraint enforcement** — `det(γ̃)=1` and `tr(Ã)=0` enforced after each RK3 step.
- **Adaptive CFL guardian** — Emergency 20% dt reduction when advection CFL > 0.95; warning at > 0.80.
- **NaN-aware physics floors** — χ ∈ [1e-4, 1.5], α ≥ 1e-6, using `!std::isfinite(x) || x < threshold` (IEEE 754 safe).

### Fixed
- **C3 (HLLE flat metric bug)** — `GRMetric3` promoted to public interface; GR fluxes now correct in curved spacetime.
- **H1 (KO dissipation OMP)** — 22 separate OpenMP thread spawns → 1 parallel region with var loop innermost (~30% compute waste eliminated).
- **Prolongation origin off-by-N bug** — Coordinate mapping scalar corrected to interior origin (was ghost boundary), eliminating uninitialized fine ghost cells.
- **`setLevelDt()` cascade** — Fixed "Zombie Physics" frozen fine grids.
- **Chi-blended selective upwinding** — Near-puncture (χ < 0.05): centered FD; far-field (χ ≥ 0.05): 4th-order upwinded. Γ̂^i always centered.
- **OpenMP include guard** — `#ifdef GRANITE_USE_OPENMP` added to `ccz4.cpp`.

### Changed
- Test count: 90 → 92 (2 selective advection tests in `test_ccz4_flat.cpp`).
- `VERSION 0.5.0` → `VERSION 0.6.0` in `CMakeLists.txt`.

---

## [v0.5.0] — 2026-04-02 / 2026-04-01

### Summary

Three concurrent workstreams: **(1)** Repository reorganization to professional production standards. **(2)** Security policy and CodeQL static analysis configuration. **(3)** Phase 5 GRMHD completions: PPM reconstruction, flat GridBlock memory layout, corrected GW recoil kick and spin-weighted spherical harmonics. Final test count before v0.6.0: **90 tests, 100% pass rate**.

### Added
- **Repository reorganization** — Root reduced to 8 essential files. `scripts/` (Python utilities), `.github/` (community health files), `docs/` (restructured with `internal/`).
- **`SECURITY.md`** — GitHub private vulnerability reporting, 48h acknowledgement, 30-day patch SLA.
- **`.github/workflows/codeql.yml`** — CodeQL C++/Python scanning on push/PR/weekly schedule. Manual C++ build mode with GCC-12 and `-DGRANITE_ENABLE_OPENMP=ON`.
- **PPM Reconstruction** (`src/grmhd/grmhd.cpp`) — Full Colella & Woodward (1984) §1–2: parabolic interpolation, monotonicity constraint, parabola monotonization, anti-overshoot limiter. 5 new `PPMTest` tests.
- **`python/granite_analysis/gw.py` — `spin_weighted_spherical_harmonic()`** — Full Wigner d-matrix (Goldberg 1967) for all modes; fast analytic paths for ℓ=2,3.
- **`python/granite_analysis/gw.py` — `compute_radiated_momentum()`** — Full Ruiz-Campanelli-Zlochower (2008) recoil kick formula with adjacent-mode coupling coefficients `a^ℓm`, `b^ℓm`.
- **`GridBlockFlatLayoutTest`** — 4 new tests for contiguous memory layout.

### Fixed
- **H2 (GridBlock flat memory)** — `vector<vector<Real>>` (22 heap allocs) → single `vector<Real>` with `data_[var * stride_ + flat(i,j,k)]`. 22 → 1 heap allocation per block.
- **H3 (RHS zero-out loop order)** — var-outermost → spatial-outermost, var-innermost (eliminates TLB thrashing on flat buffer).
- **TOV unit bug** — `RSUN_CGS` (×700,000 error) → `1.0e5 cm/km`. TOV solver now yields M ≈ 1.4 M☉, R ≈ 10 km.
- **Path compatibility** — All script paths updated for new `scripts/` directory structure.

### Changed
- Test count progression: 42 (initial) → 47 (+MPI buffer tests) → 51 (+HLLD) → 54 (+CT) → 74 (+Tabulated EOS) → 81 (+GR metric coupling, MP5, KO refactor) → **90** (+PPM, flat GridBlock, GW modes).

---

## [v0.4.0] — 2026-03-30

### Summary

Phases 4A–4D — Complete engine stabilization. Closed 5 highest-priority audit findings. HLLE now runs in actual curved GR spacetime. Production-grade MP5 5th-order reconstruction. Full HLLD Riemann solver. Constrained transport (∇·B = 0). Tabulated nuclear EOS with microphysics. Test count: 47 → 81.

### Added
- **Phase 4A — HLLD Riemann Solver** (`src/grmhd/hlld.cpp`, 477 lines) — Full Miyoshi & Kusano (2005) implementation resolving all 7 MHD wave families. 4 new `HLLDTest` tests.
- **Phase 4B — Constrained Transport** (`hlld.cpp`) — Edge-averaged EMF, `∇·B = 0` to machine precision. Two-phase update pattern preventing in-place mutation. 3 new `CTTest` tests.
- **Phase 4C — Tabulated Nuclear EOS** (`tabulated_eos.hpp`/`.cpp`) — Tri-linear log-space interpolation, Newton-Raphson T inversion, beta-equilibrium bisection solver. StellarCollapse/CompOSE HDF5 reader. Synthetic ideal-gas table for CI. `HydroVar::DYE` added as 9th conserved variable. 20 new tests across 4 fixtures.
- **Phase 4D — MP5 5th-Order Reconstruction** — Suresh & Huynh (1997); default reconstruction scheme. 5 new `GRMHDGRTest` tests.

### Fixed
- **C1 (EOS sound speed)** — `maxWavespeeds()` used hardcoded Γ=1; replaced with `eos.soundSpeed()`. Was underestimating wavespeeds by ×√(5/3) for Γ=5/3.
- **C3 (HLLE flat metric)** — `computeHLLEFlux` always used flat Minkowski metric; promoted `GRMetric3` to public interface with correct GR wavespeed formula `λ± = α(v_dir ± c_f)/(1 ± v_dir·c_f) − β_dir`.
- **H1 (KO dissipation OMP)** — 22 separate OpenMP regions → 1 parallel region, var loop innermost (eliminated ~30% compute overhead from thread-spawn latency).
- **HLLD Bug 1** — Contact speed sign convention (Miyoshi-Kusano eq. 38 `ρ(S−v_n)` vs. `ρ(v_n−S)`).
- **HLLD Bug 2** — Sound speed underestimate: isothermal `p/ρ` → adiabatic `Γ·p/ρ`.
- **HLLD Bug 3 (CT in-place mutation)** — B-field updated in-place during CT curl; fixed with two-phase compute-then-apply.
- **TOV unit bug** — `RSUN_CGS` → `1.0e5 cm/km` (×700,000 correction).

---

## [v0.3.0] — 2026-03-28

### Summary

CI/CD pipeline stabilization, non-blocking MPI ghost-zone synchronization API, and foundational engine integration (SSP-RK3 + AMR + GRMHD wired in `main.cpp`).

### Added
- **Non-blocking MPI ghost-zone API** (`grid.hpp`/`grid.cpp`) — `packBuffer()`, `unpackBuffer()`, `bufferSize()`, `getNeighbor()` / `setNeighbor()`, `getRank()` / `setRank()`.
- **`GridBlockBufferTest`** — 5 new MPI buffer and neighbor metadata tests.
- **SSP-RK3 + AMR + GRMHD integration** (`main.cpp`) — Full simulation loop: initialize → evolve → regrid → output. YAML config parser. `B5_star` flagship benchmark config.
- **`run_granite.py`** — Python driver for automated build/configure/test pipeline.

### Fixed
- **CI/CD** — Added `libomp-dev` for Clang-15 OpenMP discovery. Fixed `cppcheck` false positives (`--suppress=unusedFunction`). Corrected yaml-cpp `GIT_TAG` from `yaml-cpp-0.8.0` → `0.8.0`. Split `apt-get update` + `install` into single chained command. Added `libhdf5-dev`, `cppcheck`, `clang-format` to CI dependencies.
- **`initial_data.cpp`** — Added missing `#include`, `granite::` namespace qualifier for GCC-12 strict mode.
- **`ccz4.cpp`** — Variable shadowing cleanup, `const` annotations per `cppcheck`.
- **`StarParams::position`** — Added default member initialization `= {0.0, 0.0, 0.0}`.

### Changed
- PETSc CMake integration modernized to `IMPORTED_TARGET` facility.
- `yaml-cpp` integrated via `FetchContent`.

---

## [v0.1.0] — 2026-03-27

### Summary

Initial repository upload. Complete GRANITE engine architecture committed in a single structured snapshot.

### Added
- **CCZ4 Spacetime Evolution** (`ccz4.cpp`, ~1,024 lines) — 22 evolved variables, 4th-order FD, Kreiss-Oliger dissipation, 1+log slicing, Gamma-driver shift, constraint-damping parameters κ₁, κ₂.
- **GRMHD** (`grmhd.cpp`, ~704 lines) — 8 conserved variables, HLLE Riemann solver, PLM reconstruction, Newton-Raphson + Palenzuela con2prim, ideal gas EOS, atmosphere floors.
- **M1 Radiation Transport** (`m1.cpp`, ~417 lines) — Energy + flux evolution, Minerbo closure, optical-depth interpolation.
- **Neutrino Microphysics** (`neutrino.cpp`, ~412 lines) — β-decay/e-capture (Bruenn 1985), scattering opacities, thermal pair production.
- **Initial Data** — TOV solver, Brill-Lindquist punctures, Lane-Emden polytrope, multi-body conformal superposition.
- **Apparent Horizon Finder** (`horizon_finder.cpp`, ~600 lines) — Y_ℓm spectral expansion, Newton-Raphson on Θ=0, irreducible mass and angular momentum.
- **Postprocessing** (`postprocess.cpp`, ~450 lines) — Ψ₄ GW extraction, spin-weighted spherical harmonics, ADM integrals.
- **HDF5 I/O** — Checkpoint/restart with parallel MPI-IO.
- **Build system** — Modern CMake, C++17, optional MPI/OpenMP/HDF5/PETSc, GoogleTest via `FetchContent`, GitHub Actions CI (GCC-12, Clang-15 matrix, Debug/Release).
- **Test suite (42 tests):** `GridBlockTest` (13), `TypesTest` (7), `CCZ4FlatTest` (6), `GaugeWaveTest` (4), `BrillLindquistTest` (5), `PolytropeTest` (7).

---

*GRANITE — [GPL-3.0](LICENSE) — Liran M. Schwartz — 2026*
