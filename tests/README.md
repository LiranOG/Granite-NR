# GRANITE – Quality Assurance & Testing (tests/)

> [!NOTE]
> **Welcome to the Testing Arena.** This directory contains the complete suite of Google Test (gtest) unit and integration tests that physically enforce the mathematical correctness of the GRANITE engine. All 107 tests across 20 test suites compile and pass with **zero errors and zero warnings**.

## Directory Overview

| Test Suite / Component | Coverage Domain | Tests |
|------------------------|-----------------|:-----:|
| `amr/` | Prolongation exactness, restriction averaging, and subcycling invariants. | 5 |
| `core/` | Foundational data structures, memory layouts, and parsing rules. | 29 |
| `grmhd/` | Shock-capturing methods, C2P inversions, and ideal fluid hydrodynamics. | 37 |
| `horizon/` | Newton-Raphson finder convergence and irreducible mass geometry. | 3 |
| `initial_data/` | Analytic formulation accuracy and boundary condition compatibility. | 12 |
| `io/` | HDF5 state serialization, memory consistency, and restart capabilities. | 3 |
| `radiation/` | Grey M1 transport, variable Eddington closures, and asymptotic limits. | 4 |
| `spacetime/` | CCZ4 formulation invariants, ADM constraints, and gauge propagation. | 14 |
| `CMakeLists.txt` | Test suite build configuration. | — |

## Directory Structure

```text
tests/
├── amr/                          # test_amr_basic.cpp (5 tests)
├── core/                         # test_grid.cpp (22), test_types.cpp (7)
├── grmhd/                        # test_grmhd_gr.cpp (5), test_hlld_ct.cpp (7), test_ppm.cpp (5), test_tabulated_eos.cpp (20)
├── horizon/                      # test_schwarzschild_horizon.cpp (3)
├── initial_data/                 # test_brill_lindquist.cpp (5), test_polytrope.cpp (7)
├── io/                           # test_hdf5_roundtrip.cpp (3)
├── radiation/                    # test_m1_diffusion.cpp (4)
├── spacetime/                    # test_ccz4_flat.cpp (10), test_gauge_wave.cpp (4)
├── README.md                     # You are here
└── CMakeLists.txt                # Build rules for the test harness
```

## Test Suites (20 total)

| Suite Name | File | Tests |
|---|---|:---:|
| AMRSmokeTest | `test_amr_basic.cpp` | 5 |
| GridBlockTest | `test_grid.cpp` | 13 |
| GridBlockBufferTest | `test_grid.cpp` | 5 |
| GridBlockFlatLayoutTest | `test_grid.cpp` | 4 |
| TypesTest | `test_types.cpp` | 7 |
| GRMHDGRTest | `test_grmhd_gr.cpp` | 5 |
| HLLDTest | `test_hlld_ct.cpp` | 4 |
| CTTest | `test_hlld_ct.cpp` | 3 |
| PPMTest | `test_ppm.cpp` | 5 |
| IdealGasLimitTest | `test_tabulated_eos.cpp` | 5 |
| ThermodynamicsTest | `test_tabulated_eos.cpp` | 5 |
| InterpolationTest | `test_tabulated_eos.cpp` | 5 |
| BetaEquilibriumTest | `test_tabulated_eos.cpp` | 5 |
| SchwarzschildHorizonTest | `test_schwarzschild_horizon.cpp` | 3 |
| BrillLindquistTest | `test_brill_lindquist.cpp` | 5 |
| PolytropeTest | `test_polytrope.cpp` | 7 |
| HDF5RoundtripTest | `test_hdf5_roundtrip.cpp` | 3 |
| M1SmokeTest | `test_m1_diffusion.cpp` | 4 |
| CCZ4FlatTest | `test_ccz4_flat.cpp` | 10 |
| GaugeWaveTest | `test_gauge_wave.cpp` | 4 |

> [!IMPORTANT]
> **Test Execution:** The master test suite consists of **107 passing tests** across **20 test suites** covering all major physics modules. Run them locally via `python3 scripts/run_granite.py test` or directly with `build/bin/granite_tests`. All tests compile with zero warnings under both GCC-12 and Clang-18.
