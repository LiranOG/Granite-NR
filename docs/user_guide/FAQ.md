# GRANITE — Frequently Asked Questions

**Version:** v0.6.7 | **April 27, 2026**

---

## Table of Contents

- [Science & Physics](#science--physics)
- [Getting Started & Installation](#getting-started--installation)
- [Running Simulations](#running-simulations)
- [Understanding Simulation Output](#understanding-simulation-output)
- [HPC & Performance](#hpc--performance)
- [Contributing](#contributing)
- [Comparison with Other Codes](#comparison-with-other-codes)
- [Ecosystem & Community](#ecosystem--community)

---

## Science & Physics

**Q: What physical scenarios can GRANITE simulate in its current state (v0.6.7)?**

A: GRANITE is currently validated for single moving-puncture (single Schwarzschild black hole) stability runs and equal-mass binary black hole inspiral using Two-Punctures/Bowen-York initial data. The physics modules for GRMHD, M1 radiation, and neutrino leakage are implemented and pass unit tests, but radiation is not yet wired into the main RK3 evolution loop (planned for v0.7). Neutron star simulations require tabulated EOS support, which is planned for v0.8. The flagship B5\_star multi-SMBH scenario is the long-term production target.

**Q: How accurate is GRANITE compared to production NR codes like SpECTRE or Einstein Toolkit?**

A: GRANITE uses 4th-order finite-difference spatial discretization and SSP-RK3 time integration, which are standard in the NR community. The constraint norms achieved in v0.6.7 BBH runs (‖H‖₂ decaying to ~10⁻⁵ over 500M) are consistent with published results from comparable resolution runs in other codes. Formal convergence-order testing against the SXS waveform catalog is planned for v0.9. For precision waveform generation at the LIGO/Virgo accuracy standard, SpECTRE remains the gold standard; GRANITE's strength is the breadth of multi-physics coupling, not (yet) spectral-level phase accuracy.

**Q: Why CCZ4 and not BSSN?**

A: Both CCZ4 and BSSN are conformal decompositions of the Einstein equations that are stable for moving-puncture evolutions. CCZ4 has one key advantage: it promotes the algebraic constraint Z_μ = 0 to a dynamical field with active exponential damping (controlled by κ₁ and κ₂). This means constraint violations are actively damped rather than passively accumulated. In long-duration simulations (t > 500M), CCZ4 typically shows slower constraint growth than BSSN at identical resolution. The cost is two additional evolution variables (Θ and B^i for the shift), which is negligible.

**Q: What is the B5\_star scenario and why is it the flagship?**

A: B5\_star is a configuration of five 10⁸ M☉ supermassive black holes in a pentagon + two central ultra-massive stars. It represents a scenario that is physically plausible in dense galactic nuclei and utterly beyond the capability of any existing NR code to simulate at full physics fidelity. No other code couples CCZ4 spacetime evolution, full GRMHD, and M1 radiation transport in a single framework that can handle N > 2 simultaneous mergers. See [`docs/SCIENCE.md`](../theory/SCIENCE.md) for the full scientific context.

**Q: Can GRANITE be used for neutron star mergers?**

A: The GRMHD Valencia infrastructure and M1 radiation module are built with neutron star physics in mind. However, two components are missing for production BNS runs: (1) a tabulated nuclear EOS (planned v0.8) — current GRANITE uses an ideal-gas EOS with γ = 4/3 which is unphysical for nuclear matter, and (2) the M1 neutrino transport is not yet wired into the RK3 loop (planned v0.7). Once these are in place, BNS inspirals will be a primary validation target.

**Q: How do I interpret the Hamiltonian constraint ‖H‖₂?**

A: The Hamiltonian constraint H measures how well the evolved metric satisfies the Einstein field equations. In a perfect simulation, H = 0 everywhere at all times. In practice, discretization introduces nonzero H. A *healthy* simulation shows ‖H‖₂ that is either stable or decreasing over time — the CCZ4 constraint damping actively drives violations toward zero. A *concerning* simulation shows ‖H‖₂ growing exponentially (γ > 0.02/M sustained). See [`docs/diagnostic_handbook.md`](./diagnostic_handbook.md) for the full interpretation guide.

**Q: What is the trumpet geometry and why does it matter?**

A: In the moving-puncture method, the 1+log lapse gauge drives the slicing to a stationary "trumpet" geometry near each black hole puncture. In this geometry, the constant-time hypersurface reaches all the way from spatial infinity down to the throat of the black hole (r → 0 in the conformal coordinate), avoiding the physical singularity. The trumpet is the correct end state — if you see the lapse stabilize near a small nonzero value (~0.28–0.35 for a single BH), the gauge has converged to the trumpet. If the lapse collapses to zero and stays there, this indicates secondary gauge collapse, which is a pathology but not a numerical crash.

---

## Getting Started & Installation

**Q: Why does GRANITE require WSL2 and not run natively on Windows?**

A: The core issue is the MPI + OpenMP + HDF5 dependency stack. On Linux, these are battle-tested with known performance characteristics. On Windows, the native compilers and MPI implementations have significantly different behavior, and the OpenMP thread affinity model differs from what GRANITE's performance tuning assumes. Supporting native Windows would require a separate testing and maintenance path that diverts bandwidth from physics development. WSL2 provides Linux-native performance on Windows hardware and is the recommended path. Native Windows support is planned for v0.8.

**Q: My build fails with a HDF5 linking error. What should I do?**

A: The most common cause is a mismatch between the HDF5 version used for compilation and the one detected by CMake. Ensure you install the parallel HDF5 variant: `sudo apt install libhdf5-openmpi-dev` (not `libhdf5-dev`, which is the serial version). Then rebuild from scratch: `rm -rf build/ && python3 scripts/run_granite.py build --release`. See [`docs/INSTALL.md`](../getting_started/Installation.md) for the full troubleshooting Q&A.

**Q: The health check reports fewer OpenMP threads than my CPU has cores. What's wrong?**

A: This is usually caused by the `OMP_NUM_THREADS` environment variable being unset or set to 1. Add to your shell profile: `export OMP_NUM_THREADS=$(nproc)`. In WSL2, also ensure you are not running from a Windows-mounted path (`/mnt/c/...`) — this degrades performance and can interfere with thread affinity.

**Q: How much RAM do I need to run the B2\_eq benchmark?**

A: The B2\_eq benchmark at 64³ base resolution (4 AMR levels) requires approximately 2–3 GB RAM. At 96³ it requires approximately 8–12 GB. For development runs on a 16 GB machine, the 64³ configuration is recommended. See [`docs/BENCHMARKS.md`](./BENCHMARKS.md) for measured memory usage across all benchmark configurations.

---

## Running Simulations

**Q: My B2\_eq simulation shows no inspiral — the black holes just sit there. Why?**

A: This is the most common user error. The Bowen-York initial data with zero tangential momentum produces a *head-on* collision (or quasi-static configuration at large separation), not an inspiral. For a quasi-circular orbit at d = 10M equal-mass separation, you must set tangential momenta: `momentum: [0.0, 0.0840, 0.0]` for BH1 and `[0.0, -0.0840, 0.0]` for BH2 in `params.yaml`. The value p_t ≈ 0.0840 is derived from post-Newtonian approximation for this configuration.

**Q: My simulation crashes at t ≈ 6M with a CFL violation. How do I fix this?**

A: A crash at t ≈ 6M is the classic lapse collapse / CFL violation signature. During the gauge collapse phase, the lapse α drops to near-zero values, which makes the effective wave speed very large and violates the CFL condition. Check: (1) Is your CFL factor ≤ 0.25 globally? (2) Is the CFL at the finest AMR level ≤ 0.5? (3) Is there a lapse floor active? If all else fails, reduce the CFL factor to 0.15 and retry.

**Q: Sommerfeld boundary conditions make my constraints blow up. Is this a bug?**

A: No — this is a known documented behavior. Sommerfeld boundary conditions are **incompatible with Brill-Lindquist initial data**. The BL conformal factor has a different asymptotic structure than what Sommerfeld BCs assume. Use `boundary: type: copy` with BL data, and `boundary: type: sommerfeld` with Two-Punctures / Bowen-York data only. This is documented in the CHANGELOG as a confirmed compatibility issue.

**Q: My `ko_sigma` is set to 0.35 but my constraints are blowing up. Is this the cause?**

A: Yes, almost certainly. `ko_sigma: 0.35` causes over-dissipation that destroys the trumpet gauge profile. The safe default is `ko_sigma: 0.1`. This is a documented failure mode — always check this parameter first when debugging constraint growth. Reset to 0.1 and restart.

**Q: The AMR block count stays at 4 throughout my simulation. Is dynamic regridding working?**

A: Dynamic regridding is **fully implemented and active in v0.6.7**. Gradient-based and puncture-tracking triggers fire per step via `AMRHierarchy::subcycle()`, with iterative union-merge box aggregation and live regridding integrated into the main RK3 loop. If your block count is not changing, verify that your refinement thresholds in `params.yaml` are appropriate for your resolution.

**Q: How do I know if my simulation is scientifically valid?**

A: Use the following criteria: (1) ‖H‖₂ stable or decreasing over the run duration, (2) no NaN events, (3) CFL ≤ 0.25 globally, (4) `alpha_center` follows the expected lapse collapse → recovery trajectory, (5) AMR levels are refinig where expected (near punctures for BBH). If all five are satisfied, the simulation is healthy by the current standards. For publication-quality results, also require comparison against a SXS reference waveform — planned for v0.9.

---

## Understanding Simulation Output

**Q: What does `α_center [RECOVER]` mean in the diagnostic output?**

A: `[RECOVER]` indicates that `alpha_center` (the lapse at the grid center) has reached its minimum value after the initial collapse and is now increasing toward the trumpet equilibrium value. This is the expected, healthy behavior. The lapse lifecycle is: 1.0 (initial) → collapse to ~0.003 → recovery to ~0.03–0.35 (trumpet). `[RECOVER]` appears during the recovery phase.

**Q: What does `γ = -0.016/M` mean in the constraint output?**

A: γ is the exponential growth rate of ‖H‖₂, measured as `Δ(log ‖H‖₂)/Δt`. A negative value means the constraints are *decaying* exponentially — this is the desired behavior, indicating that the CCZ4 constraint damping is working correctly. A positive value indicates growth; values > 0.02/M sustained indicate a potential problem.

**Q: My `α_center` is stuck near 0.94 and never drops toward 0.03. Is something wrong?**

A: At dx = 0.75M (128³ base grid on ±48M domain), the trumpet geometry near the puncture is not resolved — the finest cell is 0.094M, which is comparable to the puncture scale. The code correctly evolves the gauge but cannot represent the trumpet profile at this resolution, so α does not collapse to its trumpet value. This is a resolution effect, not a bug. At finer resolution (dx_finest ≲ 0.05M), α would drop as expected. This behavior was confirmed and documented in DEV_LOG5.

**Q: How do I convert the output ‖H‖₂ values to physical units?**

A: ‖H‖₂ is dimensionless in the geometrized unit system (G = c = 1, M = 1). It has units of 1/M² where M is the total ADM mass. For a binary with M_total = 60 M☉ (like GW150914), 1/M² ≈ 1/(89 km)² ≈ 1.26 × 10⁻⁷ km⁻². For absolute comparison, always use the dimensionless ‖H‖₂ values directly.

---

## HPC & Performance

**Q: What is the recommended hardware for development runs vs. production runs?**

A: **Development runs (128³):** Any modern 6–8 core desktop with ≥16 GB RAM running Linux or WSL2. The i5-8400 reference machine completes a 500M BBH run at 64³ in ~99 minutes. **Production runs (256³+):** A cluster node with ≥128 cores and ≥128 GB RAM. Full B5\_star requires Tier-0 class hardware (~2 TB RAM, thousands of cores). **GPU runs:** H100 SXM or equivalent FP64-capable GPU, available via vast.ai or institutional allocation. The GTX 1050 Ti is not viable for FP64 GPU compute.

**Q: How do I run GRANITE on a SLURM cluster?**

A: Use `scripts/run_granite_hpc.py` to generate a SLURM job script with appropriate NUMA overrides and MPI topology. Ready-to-use job templates are in `benchmarks/scaling_tests/`. The basic command is:
```bash
python3 scripts/run_granite_hpc.py build/bin/granite_main benchmarks/B2_eq/params.yaml \
    --omp-threads 32 --mpi-ranks 128 --amr-telemetry-file /scratch/$USER/amr.jsonl
```

**Q: What is the expected strong scaling efficiency?**

A: Strong scaling has not yet been formally benchmarked beyond a single node. The target is ≥90% efficiency up to 10,000 cores for production-scale simulations. This will be measured and reported in v0.7 as part of the HPC readiness campaign.

**Q: My WSL2 simulation is much slower than expected. What's happening?**

A: The most common cause is running from a Windows-mounted filesystem (`/mnt/c/`). Always run GRANITE from a Linux-native path (e.g., `~/Granite/`). Cross-filesystem I/O in WSL2 is ~10× slower than native Linux filesystem I/O, and this bottleneck affects both build and runtime performance.

---

## Contributing

**Q: I found a bug. How do I report it?**

A: Open a GitHub Issue using the bug report template. Include: (1) your OS and compiler version, (2) the exact command that produced the error, (3) the full error output, (4) the relevant section of your `params.yaml`, and (5) the output of `python3 scripts/health_check.py`. The more context you provide, the faster the issue can be diagnosed.

**Q: I want to add a new physics module. Where do I start?**

A: Read the [Developer Guide](../developer_guide/DEVELOPER_GUIDE.md) sections on architecture and module coupling. Then open a GitHub Discussion with an [RFC] prefix describing your proposed module. This ensures alignment before you invest significant development time. The key constraint is that new modules must maintain the clean subsystem separation: new matter modules must communicate with spacetime only through `GRMetric3`, not by directly accessing CCZ4 internal variables.

**Q: Do I need to understand all of the codebase to contribute?**

A: No. The physics modules are deliberately decoupled. If you are an expert in radiation transport, you can contribute to `src/radiation/` without understanding the CCZ4 RHS loop. If you are an HPC engineer, you can work on MPI communication patterns without understanding the GRMHD Riemann solver. The `tests/` directory is the best entry point for understanding any module — read the test before the implementation.

**Q: Will my contribution be credited?**

A: Yes. All contributors are listed in `AUTHORS.md` and acknowledged in every release note. For physics or validation contributions, co-authorship on relevant papers is explicitly offered. See the Community section of the README for the full recognition tiers.

---

## Comparison with Other Codes

**Q: How does GRANITE compare to the Einstein Toolkit?**

A: The Einstein Toolkit is the most mature and community-tested NR code in existence, with a thorn ecosystem covering almost every NR scenario. GRANITE's advantages are: a clean, modern C++17 single codebase (no legacy Fortran), a real-time Python telemetry dashboard, and explicit support for N > 2 simultaneous BH mergers. GRANITE's disadvantages are: smaller community, less validation history, and no current GPU support. They are complementary, not competitive. See [`docs/COMPARISON.md`](../developer_guide/COMPARISON.md) for the full analysis.

**Q: Should I use GRANITE or SpECTRE for BBH waveform production?**

A: For high-precision waveform production targeting LIGO/Virgo template banks, SpECTRE is currently the better choice — its discontinuous Galerkin spectral methods achieve higher phase accuracy per computational cost for smooth BBH inspirals. GRANITE is the better choice when you need: (1) strong shocks or discontinuities (accretion, stellar disruption), (2) full GRMHD coupling, (3) M1 radiation transport, or (4) N > 2 simultaneous mergers.

**Q: Is GRANITE compatible with the SXS waveform catalog?**

A: GRANITE can extract Ψ₄ gravitational wave data in the same format used by SXS, and comparison against SXS catalog entries is the primary validation target for v0.9. The technical comparison infrastructure (fixed-frequency integration, extrapolation to null infinity) is implemented and tested. Actual quantitative comparison against specific SXS entries is planned as part of the v0.9 validation campaign.

---

## Ecosystem & Community

**Q: What is VORTEX and how does it relate to GRANITE?**

A: VORTEX is a standalone browser-native N-body physics engine bundled with
GRANITE at `viz/vortex_eternity/index.html`. It implements a 4th-order Hermite
predictor-corrector integrator with 1.5PN Lense-Thirring frame-dragging,
2.5PN radiation-reaction (gravitational wave inspiral), real-time GLSL
gravitational lens rendering, and gravitational wave audio sonification —
all running at 60 FPS in any modern browser with no installation required.
VORTEX currently operates as an independent post-Newtonian sandbox. The v1.0
roadmap couples VORTEX to GRANITE's HDF5 output for real-time 3D playback
of full general-relativistic simulation data.

**Q: Is there a published paper I can cite?**

A: A technical paper describing GRANITE's mathematical formalism, CCZ4/GRMHD/VORTEX
implementation, and validated benchmarks is in preparation for submission to
*Physical Review D*. The current draft (1,709 lines of LaTeX with 13 publication-
quality figures) is available in the repository at
[`docs/paper/granite_preprint_v067.tex`](../paper/granite_preprint_v067.tex)
([compiled PDF](../paper/granite_preprint_v067.pdf)).
For software citation, use the BibTeX entry in [`docs/citation.bib`](../citation.bib)
or the machine-readable `CITATION.cff` at the repository root.

---

*GRANITE v0.6.7 — FAQ — April 2026*

*This document is maintained continuously. If your question is not answered here, open a GitHub Discussion — your question will be added to the next FAQ update.*
