# GRANITE – Orchestration Scripts (`scripts/`)

> [!NOTE]
> **Welcome to the Command Center.** This directory contains the central Python CLI and auxiliary shell scripts used to orchestrate builds, run benchmarks, execute tests, and enforce repository standards for the GRANITE engine.

## Directory Overview

| Script | Responsibility |
|--------|----------------|
| `health_check.py` | Pre-flight validation of optimization flags and core topology. |
| `run_granite.py` | The master Python-based CLI. Wraps CMake, CTest, formatting, and the `dev` pipeline. |
| `run_granite_hpc.py` | Specialized wrapper for MPI/SLURM HPC environments with telemetry integration. |
| `setup_windows.ps1` | Environment provisioning script for Windows (WSL2/Conda). |

## Directory Structure

```text
scripts/
├── health_check.py               # Pre-flight environment check
├── run_granite.py                # Main execution entrypoint
├── run_granite_hpc.py            # Slurm/Cluster job submission
├── setup_windows.ps1             # Windows bootstrap
└── README.md                     # You are here
```

## Analytical Tools — Migrated to `granite_analysis`

> [!IMPORTANT]
> `sim_tracker.py`, `dev_benchmark.py`, and `dev_stability_test.py` have been **permanently removed** from this directory.
> Their logic has been refactored into the production-grade `granite_analysis` Python package under `python/granite_analysis/`.
> **Do not recreate standalone scripts here.** Git history preserves the originals if needed.

Use the following commands instead:

```bash
# Live simulation telemetry tracker
python -m granite_analysis.cli.sim_tracker [logfile] [--quiet] [--json out.json] [--csv out.csv]

# Developer benchmark dashboard
python -m granite_analysis.cli.dev_benchmark [logfile] [--benchmark single_puncture] [--quiet]

# Integrated dev pipeline (build → run → track) via the master CLI
python scripts/run_granite.py dev
python scripts/run_granite.py dev --watch   # auto-rebuild on src/ changes
```

> [!IMPORTANT]
> **Usage Rule:** All scripts must be executed from the **root of the repository**, not from within this `scripts/` directory.
> Example: `python scripts/run_granite.py build`
