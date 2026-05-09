# GRANITE – Benchmarks (benchmarks/)

> [!NOTE]
> **Welcome to the Benchmark Suite.** This directory contains standard numerical relativity scenarios and physical configurations used to rigorously validate the accuracy, stability, and performance of the GRANITE engine.

## Directory Overview

| Subdirectory/File | Purpose |
|-------------------|---------|
| `B2_eq/` | Equal-mass, non-spinning Brill-Lindquist binary black hole inspiral. The primary production benchmark. |
| `B5_star/` | Pentagon configuration of 5 supermassive black holes. |
| `gauge_wave/` | 1D and 3D gauge wave tests for verifying the CCZ4 shift evolution and coordinate conditions. |
| `scaling_tests/` | Configurations for testing MPI/OpenMP scaling efficiency. |
| `single_puncture/` | Stability test for a single moving puncture (Schwarzschild). |
| `validation_tests.yaml` | Master testing definitions for the suite. |

## Directory Structure

```text
benchmarks/
├── B2_eq/                        # Target production binary
├── B5_star/                      # Exascale multi-BH configuration
├── gauge_wave/                   # Basic coordinate stability
├── scaling_tests/                # HPC performance benchmarks
├── single_puncture/              # Single BH stability tests
├── README.md                     # You are here
└── validation_tests.yaml         # Validation master configuration
```

> [!IMPORTANT]
> **Execution Rule:** To run a benchmark, do not execute the binaries directly. Use the unified python interface from the root directory:
> ```bash
> python3 scripts/run_granite.py run --benchmark B2_eq
> ```
