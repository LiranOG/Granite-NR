# GRANITE vs. Existing NR Codes — Detailed Comparison

**Version:** v0.6.7 (current) | **April 27, 2026**

> This document provides a source-cited, capability-by-capability comparison of GRANITE against the four most widely used open-source numerical relativity frameworks: Einstein Toolkit, GRChombo, SpECTRE, and AthenaK. The goal is not to disparage these codes — they are excellent scientific instruments — but to clearly define where GRANITE fits in the landscape and what gap it addresses.

---

## Table of Contents

1. [Summary Comparison Table](#1-summary-comparison-table)
2. [Einstein Toolkit](#2-einstein-toolkit)
3. [GRChombo](#3-grchombo)
4. [SpECTRE](#4-spectre)
5. [AthenaK](#5-athenak)
6. [Conclusion: The GRANITE Niche](#6-conclusion-the-granite-niche)

---

## 1. Summary Comparison Table

| Feature | Einstein Toolkit | GRChombo | SpECTRE | AthenaK | **GRANITE** |
|---|:---:|:---:|:---:|:---:|:---:|
| **Spacetime** | | | | | |
| CCZ4 formulation | ✅ | ✅ | ✅ | ❌ | ✅ |
| BSSN formulation | ✅ | ✅ | ❌ | ❌ | ❌ |
| 4th-order FD spacetime | ✅ | ✅ | N/A (spectral) | N/A | ✅ |
| Moving punctures | ✅ | ✅ | ✅ | N/A | ✅ |
| Constraint damping (CCZ4/Z4c) | ✅ | ✅ | ✅ | N/A | ✅ |
| **Matter & MHD** | | | | | |
| GRMHD Valencia formulation | ✅ | ✅ | 🔶 | ✅ | ✅ |
| MP5 reconstruction | ✅ | 🔶 | ❌ | ✅ | ✅ |
| HLLD Riemann solver | ✅ | ❌ | ❌ | ✅ | ✅ |
| Constrained transport (∇·B=0) | ✅ | ✅ | ❌ | ✅ | ✅ |
| Tabulated nuclear EOS | ✅ | ✅ | 🔶 | ✅ | 🔶 (v0.8) |
| **Radiation** | | | | | |
| M1 radiation transport | ✅ (GR1D/IllinoisGRMHD) | ❌ | ❌ | ❌ | 🔶 (v0.7) |
| Neutrino leakage | ✅ | ❌ | ❌ | ❌ | 🔶 (v0.7) |
| Multi-group radiation | ✅ (partial) | ❌ | ❌ | ❌ | 🔶 (v0.9) |
| **AMR** | | | | | |
| Block-structured AMR | ✅ (Carpet) | ✅ | ✅ | ✅ | ✅ |
| Berger-Oliger subcycling | ✅ | ✅ | ✅ | ✅ | ✅ |
| Max AMR levels (documented) | 8–10 | 8 | 8 | 6 | 12 (target) |
| **N-Body Configurations** | | | | | |
| Binary BBH | ✅ | ✅ | ✅ | ❌ | ✅ |
| Binary BNS | ✅ | ✅ | 🔶 | ❌ | 🔶 (v0.8) |
| N > 2 BH simultaneous merger | ❌ | ❌ | ❌ | ❌ | 🔶 (v0.9) |
| **Infrastructure** | | | | | |
| Language | C/C++/Fortran | C++17 | C++17 | C++17 | C++17 |
| MPI + OpenMP | ✅ | ✅ | ✅ | ✅ | ✅ |
| GPU support | 🔶 (partial) | ❌ | 🔶 | ✅ | 🔶 (v0.7) |
| Single unified codebase | ❌ | ✅ | ✅ | ✅ | ✅ |
| Python telemetry / dashboard | ❌ | ❌ | ❌ | ❌ | ✅ |
| YAML-driven benchmarks | ❌ | ❌ | ✅ | 🔶 | ✅ |
| Containerized (Docker/Singularity) | ✅ | 🔶 | ✅ | ❌ | ✅ |
| **License** | LGPL-2.1 | MIT | MIT | BSD-3 | GPL-3.0 |
| **Test coverage** | Extensive | Moderate | Extensive | Moderate | 92 unit tests (partial — CCZ4/GRMHD/initial-data covered; AMR/horizon/M1/I/O not yet) |

✅ = Fully implemented | 🔶 = Partial / in development | ❌ = Not available

---

## 2. Einstein Toolkit

**Website:** https://einsteintoolkit.org  
**Language:** C, C++, Fortran  
**License:** LGPL-2.1  
**Architecture:** Modular "thorn" system (Cactus framework)

### Strengths

The Einstein Toolkit (ET) is the most mature and community-tested NR code in existence. It has been used for thousands of publications, is maintained by a large international consortium, and supports the broadest range of physics of any NR code through its thorn ecosystem (IllinoisGRMHD, Whisky, GR1D, etc.).

### Key Differences from GRANITE

**Architecture:** The ET's thorn system is a strength for modularity but creates friction when tightly coupling physics at the right-hand-side level. Each thorn is a semi-independent module; communication between thorns happens through shared memory, which can create subtle ordering dependencies and makes it difficult to ensure that spacetime and matter quantities are evaluated at exactly the same RK3 stage.

**Codebase age:** The ET core (Cactus framework) dates to the late 1990s. While the active thorns are modern, the underlying infrastructure carries significant legacy constraints (Fortran77 compatibility, pre-C++11 interfaces, etc.).

**N > 2 BH:** The ET's initial data infrastructure (TwoPunctures, Lorene) and gauge conditions are designed and tested for binary systems. Extending them to N > 2 simultaneous mergers requires non-trivial infrastructure work that the thorn architecture complicates.

**Python telemetry:** The ET has VisIt, Matplotlib scripts, and some Python post-processing, but no real-time dashboard during simulation runs.

---

## 3. GRChombo

**Website:** https://www.grchombo.org  
**Language:** C++17  
**License:** MIT  
**Architecture:** Single codebase, Chombo AMR library

### Strengths

GRChombo is arguably the closest architectural ancestor to GRANITE's design philosophy: a clean, modern C++17 codebase with block-structured AMR, targeted at BBH and binary neutron star mergers. It has an excellent publication record and active development by the Cambridge group.

### Key Differences from GRANITE

**Radiation:** GRChombo has no M1 radiation transport and no neutrino leakage. This limits its applicability to vacuum spacetimes and simple matter models.

**GRMHD fidelity:** GRChombo's GRMHD implementation uses simpler reconstruction (PLM/PPM) without MP5, and does not implement the HLLD solver that provides superior accuracy for MHD contact discontinuities and Alfvénic structures.

**N > 2 BH:** GRChombo's initial data and tracking infrastructure is designed for binary systems. Multi-BH configurations require external initial data solvers not natively integrated.

**Telemetry:** GRChombo provides standard HDF5 output and some Python analysis scripts but no real-time constraint monitoring dashboard.

---

## 4. SpECTRE

**Website:** https://spectre-code.org  
**Language:** C++17  
**License:** MIT  
**Architecture:** Discontinuous Galerkin (spectral element) methods

### Strengths

SpECTRE represents the state of the art in spectral numerical relativity. Its DG approach achieves exponential convergence for smooth problems, making it ideal for long inspiral simulations where accumulated phase error matters. It is the primary workhorse for the SXS Collaboration's waveform catalog.

### Key Differences from GRANITE

**Method:** SpECTRE uses discontinuous Galerkin (DG) methods, not finite differences. DG is spectrally accurate for smooth flows but handles shocks and discontinuities differently than HRSC finite-volume methods. For GRMHD with strong shocks (accretion shocks, stellar disruption), finite-volume HRSC methods (GRANITE's approach) are more directly applicable.

**GRMHD:** SpECTRE's GRMHD support is under active development as of 2026 and is not yet at production-quality for all scenarios.

**Radiation:** SpECTRE does not implement M1 radiation transport.

**Engineering overhead:** SpECTRE is one of the most sophisticated codebases in computational physics, with extensive meta-programming, task-based parallelism, and a steep learning curve. Contributing to SpECTRE requires significant C++ expertise beyond what most astrophysics researchers have.

**GRANITE's trade-off:** GRANITE uses 4th-order FD + HRSC rather than spectral methods, accepting lower order of convergence for smooth fields in exchange for robust handling of shocks and a simpler, more accessible codebase.

---

## 5. AthenaK

**Website:** [https://github.com/PrincetonUniversity/AthenaK](https://github.com/IAS-Astrophysics/athenak)  
**Language:** C++17 + Kokkos  
**License:** BSD-3  
**Architecture:** GPU-native, performance-portable (Kokkos)

### Strengths

AthenaK (the Kokkos-based successor to Athena++) is the most performant GRMHD code available. Its Kokkos abstraction layer enables near-native performance on NVIDIA GPUs, AMD GPUs, and multi-core CPUs with a single codebase. For large-scale GRMHD simulations in fixed spacetimes (e.g., accretion disc physics), AthenaK is the current performance leader.

### Key Differences from GRANITE

**Spacetime:** AthenaK does **not** evolve the spacetime dynamically. It solves GRMHD in a fixed metric background (GRFFE or metric specified analytically). This makes it inapplicable to any simulation where the spacetime itself evolves — i.e., all merger scenarios. GRANITE solves the full coupled Einstein + GRMHD system.

**N-body:** Without dynamic spacetime, AthenaK cannot simulate black hole mergers. It can simulate accretion onto a single spinning black hole, but not the inspiral and merger of multiple compact objects.

**Radiation:** AthenaK does not implement M1 radiation transport.

**GRANITE's trade-off:** GRANITE accepts lower current GPU performance in exchange for full dynamic spacetime evolution — the capability that makes merger simulations physically meaningful.

---

## 6. Conclusion: The GRANITE Niche

GRANITE is not trying to replace any of these codes. Each is excellent in its domain. The scientific niche GRANITE fills is specifically:

> **Dynamic spacetime (CCZ4) + relativistic MHD (Valencia/HRSC) + radiation transport (M1 + leakage) + N > 2 compact objects + single unified codebase**

This combination does not exist in any other open-source NR code as of April 2026. GRANITE's goal is to make it exist — rigorously, reproducibly, and with the same quality standards as the codes listed above.

The development roadmap (see [`README.md`](../README.md#roadmap)) targets:
- v0.7: GPU porting (narrowing the AthenaK performance gap)
- v0.8: Tabulated nuclear EOS (matching ET/GRChombo matter fidelity)
- v0.9: SXS catalog validation (matching SpECTRE waveform accuracy for BBH)
- v1.0: B5\_star production run (the scenario none of the above can simulate)

---

*GRANITE v0.6.7 — Code Comparison — April 2026*
*This comparison is maintained in good faith. If you identify any inaccuracies, please open a GitHub Issue tagged `[comparison]`.*
