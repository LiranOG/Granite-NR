## 📝 What This PR Does

<!-- One paragraph summary of the change and its primary motivation. -->

## 🔗 Related Issue

<!-- Link to the issue this PR addresses, e.g. "Fixes #42" or "Closes #42". -->
<!-- If there is no corresponding issue, explicitly explain why this change is necessary. -->

## 📋 Type of Change

- [ ] 🐛 Bug fix (non-breaking change that fixes an issue)
- [ ] 🚀 New feature (non-breaking change that adds functionality)
- [ ] ⚛️ Physics change (modifies numerics, governing equations, or solver behaviour)
- [ ] 🧹 Refactoring / cleanup (no behaviour change)
- [ ] 📖 Documentation update
- [ ] ⚙️ CI / build system / infrastructure change

## ✅ Pre-Flight Checklist

> [!IMPORTANT]
> All PRs must meet these stringent requirements before review.

- [ ] I have read [CONTRIBUTING.md](.github/CONTRIBUTING.md)
- [ ] `ctest --output-on-failure` passes locally (all 92 existing tests pass unconditionally)
- [ ] New functionality has a corresponding unit test (or this PR clearly explains why one is mathematically/computationally unfeasible)
- [ ] All public C++ functions feature compliant Doxygen `@brief` headers
- [ ] `clang-format-18` has been applied globally (`python3 scripts/run_granite.py format`)
- [ ] `CHANGELOG.md` has an entry under the current development version (`v0.6.7`)
- [ ] No new compiler warnings are introduced (build verified with `-Wall -Wextra -Wpedantic`)

## 🔬 Physics Validation (For Numerics/Solver Changes)

> [!NOTE]
> If this PR touches core physics kernels (CCZ4, GRMHD, AMR, horizon, postprocess), you must provide empirical validation data.

<!-- 
     - Which benchmark(s) did you run to validate the change?
     - What was the ‖H‖₂ or other constraint norm before/after?
     - Did the change affect NaN rates or overall numerical stability?
     Leave blank if this PR is strictly documentation/CI-only. 
-->

## 💻 Environment Tested

| Field | Value |
|---|---|
| **OS** | <!-- e.g. Ubuntu 22.04 / WSL2 --> |
| **Compiler** | <!-- e.g. GCC 12.3 --> |
| **MPI** | <!-- e.g. OpenMPI 4.1.2 --> |
| **CMake Build Type** | <!-- Debug / Release / RelWithDebInfo --> |
