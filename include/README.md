# GRANITE – C++ Header Definitions (include/)

> [!NOTE]
> **API Contracts and Data Structures.** This directory contains the complete public and internal C++ header (`.hpp`) definitions for the GRANITE engine. The architecture mirrors the `src/` directory to maintain strict modularity between physics subsystems.

## Directory Overview

| Component | Responsibility |
|-----------|----------------|
| `amr/` | Headers for Adaptive Mesh Refinement (Hierarchy, Subcycling, Tracking). |
| `core/` | Foundational types, Grid classes, configuration objects, and mathematical constants. |
| `grmhd/` | Structs and function declarations for matter evolution and equation of state. |
| `horizon/` | Apparent horizon tracker definitions and topological data structures. |
| `initial_data/` | Initial data loaders and analytical metric constructors. |
| `io/` | File output definitions (HDF5 wrapper, telemetry loggers). |
| `neutrino/` | Leakage schemas and interaction cross-section templates. |
| `postprocess/` | Extraction definitions for GW modes and fluid observables. |
| `radiation/` | M1 closure schemas and transport declarations. |
| `spacetime/` | CCZ4 metric variables, constraint definitions, and gauge states. |

## Directory Structure

```text
include/granite/
├── amr/
│   └── amr.hpp
├── core/
│   ├── grid.hpp
│   └── types.hpp
├── grmhd/
│   ├── grmhd.hpp
│   └── tabulated_eos.hpp
├── horizon/
│   └── horizon_finder.hpp
├── initial_data/
│   └── initial_data.hpp
├── io/
│   └── hdf5_io.hpp
├── neutrino/
│   └── neutrino.hpp
├── postprocess/
│   └── postprocess.hpp
├── radiation/
│   └── m1.hpp
└── spacetime/
    └── ccz4.hpp
```

> [!IMPORTANT]
> **Header-Only Inclusion Rule:** All external tools and Python bindings should point their `include_directories` exclusively to `include/` and import files using the `granite/` prefix (e.g., `#include "granite/core/types.hpp"`).
