# 🌌 GRANITE: The Vision for v1.0 and Beyond

> *"The true measure of a scientific tool is not how many equations it solves, but how many minds it empowers to solve them."*

GRANITE is fundamentally different from any existing Numerical Relativity (NR) or General-Relativistic Magnetohydrodynamics (GRMHD) code. It is not merely a software library; it is a **paradigm shift** in how humanity simulates and understands the universe. 

This document outlines the core philosophy, the technological roadmap to v1.0, and the sustainable scientific economy being built around this engine. This is a manifesto for the democratization of extreme astrophysics.

---

## Pillar I: Total Architectural Modularity (The YAML Revolution)

Historically, generating complex simulations in frameworks like the Einstein Toolkit or SpECTRE required an immense amount of boilerplate coding. If a scenario did not exist, researchers had to build it from scratch—navigating the deeply coupled architecture of the engine, writing custom C++ modules, and hoping the physics did not break the grid infrastructure. 

**GRANITE decouples the physics engine from the simulation scenario.**

* **Absolute Separation of Concerns:** The high-performance C++ core (`.cpp` / `.hpp` files) is strictly isolated. It handles the heavy lifting—Berger-Oliger AMR, MPI communication, CCZ4 RHS integration, and GRMHD Riemann solvers. The user *never* needs to touch this core to run a new scenario.
* **The YAML Configuration Layer:** Every aspect of a simulation—from the masses and spins of the black holes to the refinement triggers and boundary conditions—is controlled via clean, human-readable YAML files located in the `benchmarks/` directory (which will evolve into a dedicated community scenarios archive). 
* **Zero Re-Compilation Overhead:** You do not need to recompile a million-line codebase to change a physical parameter. 
* **True Accessibility:** This architecture means a first-year undergraduate physics student can explore the dynamics of Kerr black holes just as easily as a postdoctoral researcher. You do not need to be a software architect to run GRANITE; you only need spatial intelligence, a logical mindset, and a passion for physics.

---

## Pillar II: Decentralized Science & The Global Challenge Board

I believe that every individual has something to offer, and no contribution is "too small." The complexity of NR means that building a flawless engine requires a diversity of thought that no single institution possesses.

* **Open Innovation:** Whether you are an obsessive enthusiast finding a typographical error in the README, a student running a validation benchmark to catch a misplaced `-` instead of a `+`, or a theoretical physicist that piece together a new scenario—every action moves the needle.
* **The Scenario Challenge Board:** For v1.0, I plan to establish a dedicated community platform. Users will propose impossibly complex configurations (e.g., N-body chaotic interactions, extreme mass-ratio inspirals). The community will collaboratively solve these challenges, tweaking models and pushing GRANITE's capabilities further. 
* **GPL 3.0 Forever:** GRANITE will always remain 100% open-source under the GPL-3.0 license. The laws of physics belong to humanity; the tools to simulate them must remain free, transparent, and uncompromised.

---

## Pillar III: Real-Time Telemetry & Browser Visualization (VORTEX Live)

In traditional NR workflows, visualizing a binary black hole merger involves simulating for weeks, generating terabytes of HDF5 files, transferring them locally, and rendering them offline using tools like ParaView. 

**I am building the "Live Stream" model for astrophysics.**

* **The VORTEX WebGL Engine:** VORTEX is the zero-allocation, browser-native N-body and kinematic rendering engine. 
* **Live Steering & Telemetry:** In the future, GRANITE will stream a lightweight telemetry pipeline (horizon tracks, velocities, $\Psi_4$ gravitational wave strain) directly over the network via WebSockets. 
* **Browser Command Center:** While the computationally devastating AMR grid solves PDEs on remote supercomputers, any user can open their standard web browser, watch the simulation unfold in 3D real-time, and control the run parameters dynamically.

---

## Pillar IV: HPC Accessibility

Long-term, I aim to explore pathways for connecting independent 
researchers with institutional computing resources. Details are 
under research and will be announced when concrete plans exist.

---

## Pillar V: The Path to Zettascale - Photonic Computing Readiness

The traditional limits of the Von Neumann architecture—specifically the "Memory Wall" and MPI communication latency—pose insurmountable barriers for full-scale, extreme-mass-ratio inspiral (EMRI) or >3 body Supermassive Black Hole (SMBH) simulations at ultra-high resolutions. Moving electrons across silicon to fetch 31 coupled non-linear PDE variables (22 for CCZ4, 9 for GRMHD) per grid point incurs unacceptable energy and latency overheads.

To breach these limits, GRANITE is undergoing a fundamental architectural refactoring towards **Hardware-Agnostic Policy-Based Design**, specifically targeting future integration with **Photonic Processors** (e.g., Lightmatter's optical computing and interconnect fabrics).

### 1. Mixed-Precision & Abstraction
Optical coprocessors operate fundamentally as analog Mach-Zehnder Interferometer (MZI) arrays, executing Matrix-Vector Multiplications (MACs) at the speed of light but inherently at lower precisions (e.g., FP16). Numerical Relativity, conversely, requires strict `FP64` to maintain constraint damping ($\Vert H \Vert_2 \to 0$). 

GRANITE reconciles this by implementing **Mixed-Precision Iterative Refinement**:
* **Abstraction:** The underlying data types and memory layouts are abstracted via `DenseTensor3D<ComputeType, StorageType, Allocator>`.
* **Execution:** Initial elliptic constraint solving (e.g., Bowen-York initial data) will leverage the photonic fabric as an $O(1)$ low-precision preconditioner, offloading the final residual reduction to the conventional CPU in `FP64`.

### 2. Operator Dispatching & Optical Communication
The mathematical formulation (the "What") is being strictly decoupled from the execution policy (the "How"). 
* **Stencil Operations:** 4th-order Finite Difference operations are being redefined as abstract mathematical operators. On x86_64, these compile to tightly nested, SIMD-aligned loops. On a future Photonic Target, these operators will dispatch flat vectors mapped to Toeplitz matrices directly into the optical buffer.
* **Ghost Zone Synchronization:** MPI communication is abstracted behind an `ICommunicator` interface, anticipating direct optical interconnect protocols (e.g., Lightmatter Passage) for zero-latency AMR block boundary synchronization.

---

### The Road Ahead

This is not science fiction; it is an engineering roadmap. The foundation (v0.6.8) is already built, tested, and validated. As the project pushes toward GPU-porting (v0.7) and full community deployment (v1.0), the tracks for this new ecosystem are being laid.

I invite developers, physicists, HPC cluster managers, and enthusiasts to join this effort. **Do not just learn about the universe—help build the engine that simulates it.**
