# Contributing to GRANITE

Thank you for your interest in contributing to GRANITE — a numerical relativity
and GRMHD engine for extreme astrophysics. This document describes the complete
contribution workflow, coding standards, physics validation requirements, and
community expectations.

GRANITE is an open-source scientific instrument. Every contribution —
from a one-line documentation fix to a new physics module — directly advances
the state of computational astrophysics. There is no contribution too small.

---

## Table of Contents

- [Before You Begin](#before-you-begin)
- [Development Environment](#development-environment)
- [Branch Model](#branch-model)
- [Development Workflow](#development-workflow)
- [Code Standards](#code-standards)
- [Commit Message Convention](#commit-message-convention)
- [Testing Requirements](#testing-requirements)
- [Physics Validation Standards](#physics-validation-standards)
- [Pull Request Process](#pull-request-process)
- [Reporting Issues](#reporting-issues)
- [Priority Contribution Areas](#priority-contribution-areas)
- [Community Standards](#community-standards)

---

## Before You Begin

### What GRANITE needs most right now

| Priority | Area | Skills required |
|:---:|---|---|
| 🔴 Critical | **GPU CUDA/HIP kernels** for CCZ4 + GRMHD RHS | CUDA C++, GPU memory hierarchy |
| 🔴 Critical | **M1 + neutrino coupling into SSP-RK3 loop** | C++17, numerical relativity basics |
| 🔴 Critical | **GRMHD convergence tests** (Brio-Wu, OT Vortex, TOV) | GRMHD physics, Python analysis |
| 🟡 High | **`--resume` checkpoint restart CLI** | C++17, HDF5 |
| 🟡 High | **4th-order AMR prolongation** (replace trilinear) | Numerical analysis, AMR design |
| 🟡 High | **AMR reflux correction** (currently stub) | Conservation laws, AMR |
| 🟢 Useful | **SXS/ETK benchmark comparison** | NR, Python analysis |
| 🟢 Useful | **macOS / Windows native build** | CMake, cross-platform C++ |
| 🟢 Useful | **Additional initial data solvers** | GR initial value problem |
| 🟢 Useful | **Documentation and tutorials** | Technical writing |

If your background is in numerical relativity, GRMHD, HPC, or gravitational-wave
astronomy — your professional review of the physics implementation is the most
valuable thing you can offer.

---

## Development Environment

### Prerequisites

| Dependency | Minimum version | Notes |
|---|---|---|
| C++ compiler | GCC 12 or Clang 18 | C++17 required |
| CMake | 3.20+ | |
| HDF5 | 1.12.1 | Parallel (MPI-aware) build |
| OpenMPI or MS-MPI | 4.1+ | |
| yaml-cpp | 0.8.0 | |
| Python | 3.10+ | For analysis scripts |
| Google Test | 1.14 | Fetched automatically by CMake |

### Setup

```bash
# 1. Fork the repository on GitHub, then clone your fork
git clone https://github.com/<your-username>/Granite.git
cd Granite

# 2. Add the upstream remote
git remote add upstream https://github.com/LiranOG/Granite.git

# 3. Install dependencies (Ubuntu / WSL2)
sudo apt update && sudo apt install -y \
    build-essential cmake libhdf5-dev libopenmpi-dev libyaml-cpp-dev

# 4. Build (Release)
python3 scripts/run_granite.py build

# 5. Run the pre-flight health check
python3 scripts/health_check.py

# 6. Verify the full test suite passes
build/bin/granite_tests
# Expected: [  PASSED  ] 92 tests.

# 7. Install the analysis package and run the developer benchmark
pip install -e ./python[dev]
python3 scripts/run_granite.py dev
```

For Windows (WSL2), Fedora/RHEL, macOS, and HPC cluster setup, see
[`Installation.md`](https://github.com/LiranOG/Granite-NR/blob/main/docs/getting_started/Installation.md) for platform-specific instructions.

---

## Branch Model

GRANITE follows a structured Git Flow:

```
main          ← stable releases only (tagged, v0.6.5, v0.6.7, ...)
develop       ← integration branch for the next release (v0.7.0)
feature/*     ← individual feature development
bugfix/*      ← targeted bug fixes
release/*     ← release preparation and stabilisation
hotfix/*      ← urgent fixes to main
```

**Key rules:**
- Never push directly to `main`. All changes go through Pull Requests.
- `main` is a protected branch — only merged via PR after CI passes.
- All new work branches from `develop`, not `main`.
- Branch names use `kebab-case`: `feature/m1-rk3-coupling`, `bugfix/amr-reflux-nan`.

---

## Development Workflow

```bash
# 1. Sync your fork with upstream
git fetch upstream
git checkout develop
git merge upstream/develop

# 2. Create your feature branch
git checkout -b feature/your-feature develop

# 3. Develop, commit incrementally (see commit convention below)
# ... write code, tests, docs ...

# 4. Keep your branch current
git fetch upstream
git rebase upstream/develop

# 5. Ensure all tests pass
cd build && ctest --output-on-failure && cd ..

# 6. Auto-format all C++ before pushing
python3 scripts/run_granite.py format

# 7. Push and open a Pull Request against develop
git push origin feature/your-feature
```

---

## Code Standards

### Language and compiler

- **Core physics**: C++17. No C++20 features without discussion.
- **GPU kernels**: CUDA C++ (sm_80+ for H100 target) or HIP for portability.
- **Analysis and tooling**: Python 3.10+, type hints encouraged.
- All code must compile **without warnings** under
  `-Wall -Wextra -Wpedantic` on both GCC-12 and Clang-18.

### Naming conventions

| Entity | Convention | Example |
|---|---|---|
| Classes / structs | `PascalCase` | `CCZ4Evolution`, `AMRHierarchy` |
| Functions / methods | `camelCase` | `computeRHS()`, `regrid()` |
| Variables | `snake_case` | `conformal_factor`, `block_id` |
| Constants / enums | `UPPER_SNAKE_CASE` | `MAX_REFINEMENT_LEVELS` |
| Namespaces | `lowercase` | `granite::spacetime`, `granite::grmhd` |
| Template parameters | `PascalCase` | `template <typename RealType>` |
| Files | `snake_case` | `ccz4.cpp`, `hdf5_io.hpp` |

### Code structure

- **Headers** (`include/granite/<subsystem>/`) contain public interfaces only.
  Implementation details live in `src/`.
- **Every public class and function** must have a Doxygen comment block:

```cpp
/**
 * @brief Compute the CCZ4 right-hand side at all interior grid points.
 *
 * Implements the full conformal and covariant Z4 evolution equations of
 * Alic et al. (2012), PhysRevD 85 064040, including:
 *   - Conformal Ricci tensor via Christoffel symbols
 *   - Physical Laplacian of the lapse (exact conformal decomposition)
 *   - Chi-blended upwind advection near the puncture
 *   - Fifth-order Kreiss-Oliger dissipation
 *
 * @param[in]  grid     Input GridBlock containing current evolved variables.
 * @param[out] rhs      Output GridBlock to receive the computed time derivatives.
 */
void CCZ4Evolution::computeRHS(const GridBlock& grid, GridBlock& rhs) const;
```

- **No magic numbers.** All physical parameters and numerical coefficients
  must be named constants with a comment citing their origin:

```cpp
// Constraint damping: Alic+ (2012), Table I, recommended value
constexpr Real kappa1_default = 0.02;

// KO dissipation: 7th-order undivided difference coefficients
constexpr std::array<Real, 7> ko_coeffs = {1, -6, 15, -20, 15, -6, 1};
```

- **Memory management**: use RAII throughout. No raw `new`/`delete`.
  Prefer `std::vector`, `std::unique_ptr`, and `std::array`.
- **No global mutable state.** All simulation state lives in `SimulationParams`
  or the AMR hierarchy.

### Physics implementation

Every physics formula must cite its source in a comment immediately above
the implementation:

```cpp
// Physical Laplacian from conformal decomposition:
//   D^k D_k α = χ · γ̃^{ab}[∂_a∂_b α − Γ̃^k_{ab} ∂_k α]
//             − ½ γ̃^{ab} ∂_a χ ∂_b α
// Derivation: Baumgarte & Shapiro (2010), eq. 3.122
// NOTE: coefficient is −1/2, NOT +1/4. The +1/4 variant causes
// gauge collapse at step ~80 in single-puncture runs.
Real physical_lap_alpha = chi * laplacian_alpha - 0.5 * chi_grad_alpha;
```

---

## Commit Message Convention

GRANITE follows the [Conventional Commits](https://www.conventionalcommits.org/)
specification. Every commit must have a structured message:

```
<type>(<scope>): <short imperative description>

[optional body — explain WHY, not WHAT]

[optional footer: references, breaking changes]
```

### Types

| Type | When to use |
|---|---|
| `feat` | A new feature or capability |
| `fix` | A bug fix |
| `test` | Adding or modifying tests |
| `docs` | Documentation changes only |
| `perf` | Performance improvement |
| `refactor` | Code restructuring without behaviour change |
| `ci` | CI/CD pipeline changes |
| `chore` | Build system, dependency, or tooling updates |

### Scopes

`ccz4`, `grmhd`, `amr`, `m1`, `neutrino`, `horizon`, `initial_data`,
`io`, `postprocess`, `vortex`, `scripts`, `tests`, `docs`, `ci`, `build`

### Examples

```
feat(m1): wire M1Transport::computeRHS into SSP-RK3 evolution loop

Adds explicit operator-split M1 substep between GRMHD and spacetime
in main.cpp. The M1 module was previously compiled and unit-tested but
not called from the time loop. Radiation back-reaction on the metric
is not yet included (scheduled for v0.8).

Closes #42
```

```
fix(ccz4): correct physical Laplacian coefficient from +0.25 to -0.5

The +0.25 coefficient caused secondary gauge collapse at step ~80 in
single-puncture runs, driving alpha_center to zero and producing NaN
cascades by step 100. The correct coefficient is -1/2 per
Baumgarte & Shapiro (2010) eq. 3.122.

Fixes #17
```

```
test(grmhd): add Brio-Wu MHD shock tube convergence test

Validates the HLLD Riemann solver against the exact solution of
Brio & Wu (1988), J. Comput. Phys. 75, 400. Tests L1 norm of
density error at t=0.1 for 64, 128, and 256 cells, verifying
first-order convergence in the presence of shocks.
```

---

## Testing Requirements

### Running the test suite

```bash
cd build

# Full suite (all 92 tests, ~60 seconds)
ctest --output-on-failure

# Single module
ctest -R spacetime --output-on-failure
ctest -R grmhd --output-on-failure

# Convergence tests (longer, up to 600 s)
ctest -R convergence --timeout 600 --output-on-failure

# Verbose output for debugging
ctest --output-on-failure -VV -R ccz4
```

### Requirements for new contributions

Every new feature or bug fix **must** include tests. A Pull Request that
adds physics code without tests will not be merged.

| Contribution type | Required tests |
|---|---|
| New physics formula | Unit test on flat/analytic background |
| New reconstruction scheme | Accuracy test against exact smooth profile |
| New Riemann solver | Shock-tube test with known exact solution |
| New initial data | Constraint satisfaction test at t=0 |
| Bug fix | Regression test that would have caught the bug |
| Performance change | Before/after throughput benchmark |

### Writing tests

```cpp
// Place in: tests/<module>/test_<feature>.cpp
// Framework: Google Test (gtest)

#include <gtest/gtest.h>
#include "granite/grmhd/grmhd.hpp"

// Test fixture for shared setup
class BrioWuTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialise grid, set Brio-Wu ICs, etc.
    }
};

TEST_F(BrioWuTest, DensityL1ConvergesFirstOrder) {
    // Test body — must complete in < 60 s
    // Use EXPECT_NEAR, EXPECT_LT, not ASSERT_ (allows all failures to print)
    EXPECT_LT(l1_error_fine / l1_error_coarse, 2.2);  // ~1st order in shocks
}
```

**Test execution time limits:**
- Unit tests: < 60 seconds each
- Convergence tests (tagged with `convergence` in CMake): < 600 seconds

---

## Physics Validation Standards

GRANITE is a scientific instrument. Physics correctness is held to the same
standard as peer-reviewed publication.

### Required for any new physics module

1. **Governing equations with citations** in the Doxygen header.
2. **Unit test on a known analytic solution** (flat spacetime, TOV star,
   linear wave, etc.).
3. **Convergence test** demonstrating the expected order of accuracy
   (2nd-order for finite-volume, 4th-order for finite-difference stencils).
4. **Known limitations documented** — if the module has an integration status
   other than fully wired into the RK3 loop, this must be stated explicitly
   in the Doxygen header and in `docs/known_limitations.md`.

### Validation benchmarks

New spacetime code must pass:

| Benchmark | Expected result |
|---|---|
| Flat spacetime RHS = 0 | ‖RHS‖ < 10⁻¹⁴ (machine precision) |
| Gauge wave propagation | ‖H‖₂ remains bounded for t ≥ 100 M |
| Single Schwarzschild puncture | ‖H‖₂ decreasing monotonically; α converges to trumpet |
| Equal-mass BBH inspiral | ‖H‖₂ < 10⁻³ through t = 500 M; zero NaN events |

New GRMHD code must pass:

| Benchmark | Expected result |
|---|---|
| Brio-Wu MHD shock tube | 1st-order L1 convergence in density |
| Orszag-Tang vortex | Correct energy power spectrum at t = 0.5 |
| TOV star oscillation | Fundamental mode frequency within 1% of analytic |
| ∇·B = 0 preservation | ‖∇·B‖ < 10⁻¹² throughout evolution (CT enforcement) |

---

## Pull Request Process

### Before opening a PR

- [ ] `ctest --output-on-failure` passes on your machine
- [ ] `python3 scripts/run_granite.py format` applied (no formatting diffs)
- [ ] New tests written and passing
- [ ] Doxygen comments on all new public APIs
- [ ] Physics formulas cited in comments
- [ ] `CHANGELOG.md` updated with your change under the `[Unreleased]` section

### PR description template

```markdown
## Summary
<!-- One paragraph: what does this PR do and why? -->

## Physics basis
<!-- Equation numbers and references for any new physics formulas -->

## Tests added
<!-- List new test files and what they verify -->

## Validation
<!-- Which benchmarks were run? What were the results? -->

## Known limitations
<!-- Anything explicitly not addressed in this PR -->

## Checklist
- [ ] All existing tests pass
- [ ] New tests added
- [ ] Documentation updated
- [ ] No compiler warnings (-Wall -Wextra -Wpedantic)
- [ ] clang-format applied
- [ ] CHANGELOG.md updated
```

### Review process

1. CI must pass (build matrix + all 92 tests + clang-format + cppcheck).
2. At least one review from a core developer or domain expert.
3. Physics PRs (new formulations, new solvers) require a review comment
   confirming the governing equations match the cited references.
4. Squash merge into `develop` after approval.
5. Branch is deleted after merge.

---

## Reporting Issues

Use GitHub Issues with the appropriate label:

| Label | Use for |
|---|---|
| `bug` | Incorrect behaviour — include steps to reproduce and expected vs actual output |
| `physics` | Incorrect physics — include the equation and citation for the correct formula |
| `performance` | Unexpected slowdown — include timing data and hardware spec |
| `enhancement` | Feature request — describe the use case and expected interface |
| `documentation` | Missing, unclear, or incorrect documentation |
| `question` | Usage questions before opening a discussion |
| `partnership` | Institutional collaboration or HPC allocation inquiries |

### Bug report template

```markdown
**GRANITE version:** v0.6.7 (commit abc1234)
**OS / compiler:** Ubuntu 22.04 WSL2, GCC 12.3
**Benchmark / config:** `benchmarks/B2_eq/params.yaml`

**Describe the bug:**
<!-- What happened? -->

**Steps to reproduce:**
1. `python3 scripts/run_granite.py build`
2. `python3 scripts/run_granite.py run --benchmark B2_eq`
3. At step 42, ‖H‖₂ exceeds 1.0 and NaN events appear

**Expected behaviour:**
<!-- What should happen according to docs/diagnostic_handbook.md? -->

**Log output:**
<!-- Paste the relevant lines from dev_logs/ -->

**Additional context:**
<!-- Health check output, hardware specs, etc. -->
```

---

## Priority Contribution Areas

### If you work in numerical relativity or GRMHD

- Validate the CCZ4 implementation against your group's published results
- Run the BBH benchmark and compare ‖H‖₂ trajectories against ETK or GRChombo
- Review the GRMHD Valencia formulation in `src/grmhd/grmhd.cpp` for
  correctness of the source terms and con2prim algorithm
- Contribute initial data configurations for exotic scenarios (EMRI, N-BH,
  magnetised stars)

### If you work in HPC or GPU computing

- Port the CCZ4 RHS and GRMHD flux loops to CUDA/HIP kernels
- Profile the OpenMP scaling on 16+ core nodes and identify the synchronisation
  bottleneck at AMR level boundaries
- Test the SLURM/PBS templates in `benchmarks/scaling_tests/` on your cluster
  and report throughput and strong-scaling efficiency

### If you work in gravitational-wave astronomy

- Compare the Ψ₄ extraction output against SXS waveform catalogue entries
- Validate the fixed-frequency strain integration against an independent
  implementation
- Run the BBH benchmark with non-zero spins and compare ringdown frequencies
  to quasi-normal mode tables

### If you are a student or independent contributor

- Fix a documentation error or improve an explanation in `docs/`
- Add a unit test for an untested code path
- Improve the error messages and phase classification logic in `granite_analysis/utils/ui.py`
- Translate a `docs/` page or write a tutorial for a specific use case
- Run `B2_eq` on your hardware and report the benchmark numbers via a GitHub Issue

---

## Community Standards

GRANITE adheres to the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md).
All contributors are expected to uphold a professional, respectful, and
scientifically rigorous environment.

**Scientific integrity is non-negotiable.** If you find an error in the
physics — a wrong sign, a missing term, an incorrect reference — please
report it immediately via a `physics` issue. A wrong formula caught early
is worth a thousand correct ones discovered late.

---

> [!NOTE]
> GRANITE is actively seeking collaborators from numerical relativity groups,
> gravitational-wave observatories (LIGO, Virgo, LISA), and Tier-1/2
> supercomputing centres.
>
> For institutional partnership and HPC allocation inquiries,
> open a GitHub Issue tagged `[partnership]`.
>
> Questions about the physics, the code, or how to get started?
> Open a GitHub Discussion — every question is welcome.
>

***Let's build the ultimate extreme-astrophysics engine — together.***
