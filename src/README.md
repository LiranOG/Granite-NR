# GRANITE – Core Source (src/)

> [!NOTE]
> **Welcome to the Engine Core.** This directory contains the complete C++ source code for the GRANITE numerical relativity engine. The architecture strictly separates physics domains into distinct, modular directories.

## Directory Overview

| Component | Responsibility |
|-----------|----------------|
| `amr/` | Adaptive Mesh Refinement logic (Berger-Oliger, Prolongation, Restriction). |
| `core/` | Foundational infrastructure (Grid definitions, YAML parameter parsing, Utilities). |
| `grmhd/` | Valencia formulation GRMHD solvers, Riemann problems, and constrained transport. |
| `horizon/` | Apparent horizon tracker (Newton-Raphson elliptic solver on the grid). |
| `initial_data/` | Puncture setup (Brill-Lindquist, Two-Punctures) and TOV star generation. |
| `io/` | High-performance parallel data serialization (HDF5). |
| `neutrino/` | Neutrino leakage and transport integration. |
| `postprocess/` | Diagnostics, telemetry, and GW extraction routines. |
| `radiation/` | Grey M1 radiation transport (Currently decoupled from main RK3 loop). |
| `spacetime/` | The core BSSN/CCZ4 spacetime evolution kernels and gauge conditions. |
| `main.cpp` | Main entry point for the C++ engine. |

## Directory Structure

```text
src/
├── amr/                          # Mesh geometry and hierarchy
├── core/                         # Memory layouts and global state
├── grmhd/                        # Matter physics
├── horizon/                      # Black hole tracking
├── initial_data/                 # t=0 configurations
├── io/                           # Disk operations
├── neutrino/                     # Neutrino microphysics
├── postprocess/                  # Observables and logging
├── radiation/                    # M1 neutrino/photon transport
├── spacetime/                    # CCZ4 spacetime physics
├── README.md                     # You are here
└── main.cpp                      # Simulation orchestrator
```

> [!IMPORTANT]
> **Compilation Rules:** Do not invoke `cmake` manually from this directory. Always use the top-level build orchestrator: `python3 scripts/run_granite.py build --release`. 
> All new public C++ functions added here MUST include Doxygen `@brief` headers.
