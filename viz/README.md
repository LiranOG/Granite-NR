# viz/

This directory contains post-processing plotting and visualization scripts.

## Contents

| Script | Status | Description |
|---|---|---|
| *(future)* `plot_constraints.py` | planned | Plot `‖H‖₂` and `α_center` from HDF5/log output. |
| *(future)* `plot_gw.py` | planned | Plot Ψ₄ gravitational wave strain from extracted data. |

For the current Python analysis package (GW extraction, HDF5 readers, matplotlib wrappers),
see [`python/granite_analysis/`](../python/granite_analysis/).

----

## 🌀 VORTEX: The WebGL Visualization Engine

Located in the `vortex_eternity/` directory, **VORTEX** is a highly optimized, zero-allocation N-Body visualization and simulation engine designed to run natively within a standard web browser. 

**What, How, and Why:**
* **Why it was built:** VORTEX was conceived to eliminate the friction typically associated with massive HPC deployments. It provides researchers and developers with an immediate, visually intuitive, yet mathematically rigorous environment to explore strong-field astrophysics (e.g., Tidal Disruption Events, binary inspirals) without requiring compilation or cluster access.
* **What it is:** It is a standalone, browser-based sandbox that bridges the gap between interactive 3D WebGL rendering and hardcore computational physics.
* **How it was built:** VORTEX enforces strict HPC data-oriented paradigms within JavaScript. To achieve a flawless 60 FPS while computing complex physics, it utilizes a pure **zero-allocation inner loop** to completely bypass Garbage Collection stalls. The physics engine employs analytical Post-Newtonian expansions (1PN kinematics, 1.5PN Lense-Thirring spin-orbit coupling via exact Rodrigues' rotation, and 2.5PN radiation reaction). To ensure symplectic stability, it integrates the equations of motion using a **4th-Order Hermite Predictor-Corrector**, stabilized by Aarseth adaptive timestepping, Kahan compensated summation, and a multi-level NaN-shielding bisection recovery mechanism.

**The v1.0 Architectural Roadmap (The Playback Viewer):**
While VORTEX currently solves its own PN approximations, its ultimate architectural destiny is to serve as the unified frontend for GRANITE. For the v1.0 release, the impossibly heavy PDE workloads (full CCZ4 spacetime evolution and GRMHD on AMR grids) will remain exclusively within the C++ backend. The Python toolchain (`python/granite_analysis/`) will distill GRANITE's massive HDF5 outputs into lightweight JSON files. VORTEX will then ingest these datasets, functioning as a hyper-accurate **3D Playback Dashboard**, rendering exact supercomputer trajectories and gravitational wave strains ($\Psi_4$) seamlessly in the browser.

**The Ultimate Vision: Real-Time In-Situ Steering (Pending Support):**
Should resources, community backing, and funding permit, the maximalist goal is to evolve VORTEX into a **Full Real-Time Interactive Command Center**. By wrapping the GRANITE backend with a lightweight WebSocket server, VORTEX would stream the live simulation directly from the HPC cluster. This would allow two-way, on-the-fly interaction—enabling users to pause, inject matter, or steer the AMR grids dynamically in full 3D while the cluster computes in the background. If this real-time paradigm proves too resource-intensive to maintain, the Playback Viewer model described above will firmly stand as the uncompromised, winning alternative.
