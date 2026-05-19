# GRANITE: Technical Vision for v1.0 and Beyond

GRANITE is being developed as an open-source numerical relativity and general-relativistic magnetohydrodynamics research code with an emphasis on modular configuration, reproducible scenarios, high-performance execution, and accessible visualization.

This document describes the intended direction from the current v0.6.8 foundation toward a v1.0 community-facing release and longer-term hardware-readiness goals. It should be read as a technical roadmap rather than a claim that every capability is already implemented. Several parts of the plan are ambitious and will require sustained engineering, physics validation, documentation, and external collaboration.

The core objective is practical: reduce the amount of custom systems programming required to define, run, inspect, and share complex simulations, while preserving the numerical discipline expected from a serious NR/GRMHD codebase. Achieving that objective is beyond the realistic capacity of one developer alone. It will require contributions from numerical relativists, computational physicists, HPC engineers, GPU programmers, cluster administrators, scientific visualization developers, documentation maintainers, and users willing to test the software against known benchmarks.

---

## Pillar I: User-Defined Simulations via YAML Configuration

A central design goal for GRANITE is to separate the numerical engine from the description of a simulation scenario. In many established NR and GRMHD workflows, changing a scenario can require direct modification of compiled code, framework-specific configuration files, or additional glue code. GRANITE’s intended v1.0 workflow is to make the scenario definition explicit, inspectable, and shareable through structured YAML configuration.

This does not remove the underlying numerical complexity. It moves a well-defined subset of that complexity into a declarative layer, where parameters, models, refinement choices, boundary conditions, and output requests can be reviewed without editing the C++ core.

### Planned separation of responsibilities

The high-performance C++ core is intended to remain responsible for the numerically intensive components:

- Berger-Oliger adaptive mesh refinement and refinement scheduling.
- MPI communication and domain decomposition across distributed memory systems.
- CCZ4 right-hand-side integration and constraint damping.
- GRMHD flux handling, reconstruction, and Riemann solver interfaces.
- Memory layout, ghost-zone management, checkpointing, and output infrastructure.
- Runtime diagnostics needed for stability monitoring and benchmark comparison.

The YAML layer is intended to define the simulation problem without requiring users to modify these internal mechanisms. A user should be able to specify the physical setup, numerical parameters, diagnostics, and output policy in a file that can be versioned, reviewed, and shared.

The path to this design requires more than a parser. A complete implementation will need schema validation, default handling, unit conventions, parameter-range checks, clear error messages, test cases for malformed files, and a mapping from human-readable settings to safe internal structures. The main engineering challenge is to provide flexibility without allowing invalid or physically inconsistent configurations to enter the solver silently.

### Scope of the YAML configuration system

The intended YAML system should eventually cover the full scenario definition, including:

- Black-hole masses, spins, positions, and momenta where applicable.
- Initial-data selection, including support for standard binary and exploratory configurations.
- Mesh hierarchy, refinement triggers, block sizes, prolongation policies, and regridding cadence.
- Gauge choices, evolution parameters, dissipation settings, and constraint-damping parameters.
- Boundary conditions and outer-domain geometry.
- GRMHD model parameters, where relevant, including fluid variables, magnetic-field initialization, atmosphere treatment, and equation-of-state selection.
- Diagnostic outputs, checkpoint intervals, waveform extraction settings, and visualization telemetry requests.
- Validation or benchmark metadata, so that a scenario can declare what reference behavior it is expected to reproduce.

The existing `benchmarks/` directory can serve as the initial location for such examples and may later evolve into a curated scenario archive. For that archive to be useful, it will need community-maintained metadata: supported GRANITE version, expected runtime scale, hardware assumptions, known limitations, and reference plots or numerical tolerances.

### Recompilation and reproducibility goals

The intended user experience is that changing a physical parameter, grid setting, or output request should not require recompiling the full codebase. This is a practical software-engineering goal, not a claim that every change can be made safely at runtime. Some changes will still require compile-time options, backend selection, or changes to numerical kernels. The v1.0 target is to make common scenario-level changes declarative and reproducible.

A complete YAML-driven workflow would improve reproducibility by making a run’s scientific intent visible in a compact file. The configuration file should be sufficient to reconstruct the scenario, identify the solver options, and compare results across hardware platforms or code versions. Reaching that point will require disciplined versioning of schemas and careful compatibility management as the code evolves from v0.6.8 to v1.0.

### Intended users and contribution needs

The long-term aim is to support a range of users: students learning numerical relativity, researchers testing scenario variants, engineers benchmarking infrastructure, and contributors adding new physics modules. The interface should be approachable, but the scientific validity of a simulation still depends on correct equations, stable numerics, convergence behavior, boundary choices, and benchmark comparison.

This pillar will require contributors who can work on schema design, documentation, validation examples, parser robustness, benchmark scenarios, and numerical sanity checks. Making configuration easier is only useful if the resulting simulations remain physically interpretable.

---

## Pillar II: Distributed Open Development and Community Challenge Board

GRANITE’s development model should treat the project as a collaborative scientific software effort rather than a single-person code release. Numerical relativity and GRMHD involve enough physics, numerical analysis, infrastructure, visualization, and documentation work that a credible v1.0 release will require external review and contribution.

The planned community model has two related components: open-source development under GPL-3.0 and a structured challenge board for scenario proposals, validation tasks, and engineering problems.

### Open contribution model

Contributions may include small corrections, documentation improvements, benchmark runs, test failures, reproducibility reports, numerical comparisons, GPU kernels, visualization tools, or new physics scenarios. Small contributions are useful when they reduce ambiguity, expose bugs, improve examples, or make the code easier for another researcher to run.

A serious open-development process will need:

- Contribution guidelines that separate documentation changes, numerical changes, infrastructure changes, and physics-model changes.
- Issue templates for bugs, benchmarks, performance regressions, and scenario proposals.
- A review process for code that affects numerical correctness.
- Continuous integration for unit tests, parser tests, and reduced-size benchmark checks.
- A policy for reporting results, including compiler versions, hardware, MPI configuration, and input YAML files.
- Maintainer roles, triage procedures, and a clear distinction between experimental branches and release-quality functionality.

The goal is not to present community activity as proof of correctness. The goal is to create a workflow where errors can be found, reproduced, discussed, and fixed by people with relevant expertise.

### Scenario Challenge Board

For v1.0, GRANITE should support a dedicated community challenge board. The board would allow users to propose simulation scenarios or technical tasks and classify them by difficulty, required physics, expected hardware scale, and validation status.

Possible challenge categories include:

- Reproduction of known validation tests.
- Binary black-hole scenarios with specified mass ratios and spins.
- GRMHD accretion or magnetized-flow test problems.
- Extreme mass-ratio inspiral exploratory configurations.
- Multi-body or chaotic interaction experiments, clearly marked as exploratory unless validated.
- AMR stress tests, ghost-zone communication tests, and scalability benchmarks.
- Visualization and telemetry tasks for runtime inspection.
- Documentation challenges, such as making a benchmark reproducible on a new cluster.

The board should avoid presenting every proposed scenario as physically validated. Each entry should carry labels such as “educational,” “benchmark,” “experimental,” “requires validation,” or “research-grade only after convergence testing.” This helps prevent a common failure mode in simulation projects: mistaking an interesting configuration for a trustworthy scientific result.

### Why the community is structurally necessary

The scope of GRANITE crosses several professional domains. Numerical relativists are needed to assess formulations, constraints, gauge behavior, and benchmark relevance. HPC engineers are needed to evaluate scaling, MPI behavior, memory layout, job scheduling, and profiling. GPU programmers are needed for portable acceleration paths. Cluster administrators are needed to make deployment realistic on institutional systems. Interface designers and visualization developers are needed to make runtime telemetry usable without compromising scientific meaning.

No single contributor is likely to cover all of these areas at production quality. The challenge board should therefore function as a coordination mechanism: it should make missing work visible, allow contributors to select bounded tasks, and keep ambitious goals connected to testable milestones.

### GPL-3.0 commitment

GRANITE is intended to remain open-source under the GPL-3.0 license. This licensing choice supports transparency, redistribution, modification, and review. It also means that future ecosystem work—scenario archives, benchmark configurations, visualization integrations, and cluster-deployment recipes—should be organized so that users can inspect and reproduce the technical basis of a result.

---

## Pillar III: Runtime Telemetry and Browser-Based Visualization via VORTEX/WebGL

Large NR and GRMHD simulations often produce substantial output files that are inspected after a run using offline visualization tools. That workflow remains important and should not be replaced where full-resolution analysis is required. GRANITE’s additional goal is to provide a lightweight runtime telemetry path so that selected simulation diagnostics can be viewed during execution through a browser interface.

The proposed VORTEX integration is intended to support browser-native visualization based on WebGL, with a focus on reduced diagnostic streams rather than full volumetric data transfer. The design target is not to stream an entire AMR hierarchy to a laptop. The target is to expose compact, scientifically relevant summaries that help users monitor a run and notice problems earlier.

### VORTEX as a browser visualization layer

VORTEX is planned as a browser-based rendering layer for selected N-body, kinematic, and diagnostic quantities. In the GRANITE context, it should display derived or reduced data such as:

- Apparent-horizon tracks or candidate horizon-location diagnostics.
- Object trajectories, coordinate velocities, and separation measures.
- Waveform extraction quantities, including Ψ₄ and, where appropriate, derived strain estimates.
- Constraint-norm trends and other stability indicators.
- Refinement-region summaries and coarse AMR structure.
- Selected scalar time series relevant to runtime monitoring.
- Scenario metadata and run status.

This visualization layer should be treated as an inspection and steering interface, not as a substitute for full scientific post-processing. Any quantity shown in the browser must be documented: how it is computed, at what cadence, with what resolution reduction, and whether it is a coordinate diagnostic, gauge-dependent measure, or physically interpretable observable.

### Telemetry pipeline

The intended architecture is a lightweight telemetry pipeline from the simulation process to a browser client, likely using WebSockets or a similar streaming protocol. The computational solver would continue to run on a remote workstation or supercomputing cluster, while a reduced stream of diagnostics is transmitted to the user interface.

A credible implementation will require:

- A stable telemetry schema.
- Rate limiting so visualization does not interfere with solver performance.
- Serialization formats that are compact and versioned.
- Clear separation between solver state and UI state.
- Authentication or access control for remote runs when deployed on shared systems.
- Failure handling if the browser disconnects or the telemetry stream drops.
- Tests ensuring that telemetry extraction does not perturb numerical results.
- Documentation explaining which quantities are approximate monitoring signals and which are suitable for later analysis.

The engineering risk is that real-time visualization can become expensive or misleading if it attempts to carry too much data. GRANITE should therefore prioritize low-bandwidth diagnostics first, then add richer visualization only after profiling shows that the solver remains unaffected.

### Live steering

The longer-term plan includes controlled live steering of selected run parameters. This capability must be designed conservatively. Changing numerical parameters during a simulation can affect reproducibility and scientific interpretation. Some parameters may be safe to adjust at runtime, such as output cadence, visualization detail, or diagnostic selection. Others, such as gauge settings, refinement policies, or physical model parameters, require careful rules, logging, and possibly restrictions.

A responsible live-steering implementation should log every runtime change, include those changes in the final run metadata, and distinguish between monitoring-only sessions and scientifically reportable runs. The development path will require collaboration between numerical experts, systems engineers, and interface designers.

### Role of WebGL

WebGL is the practical target for browser rendering because it allows cross-platform visualization without requiring a custom native client. The first implementation should focus on stable rendering of trajectories, diagnostic curves, and simplified 3D objects. Full AMR-volume visualization in-browser is a separate and more demanding problem involving data reduction, level-of-detail methods, memory limits, and network throughput.

This pillar therefore has two stages: first, telemetry and simplified WebGL monitoring; later, richer browser visualization if the data model, performance budget, and scientific semantics are clear.

---

## Pillar IV: HPC Accessibility and Operational Deployment Pathways

GRANITE’s long-term usefulness depends not only on numerical kernels but also on whether users can build, run, and reproduce simulations on real computing systems. Numerical relativity and GRMHD workloads can quickly exceed the resources available on ordinary personal machines. For that reason, HPC accessibility is a necessary part of the roadmap.

This pillar has two meanings. First, the code should be technically deployable on common research-computing environments. Second, the project should explore realistic ways for independent researchers or smaller groups to access compute resources without overstating what is currently available.

### Deployment requirements

A credible HPC deployment path should include:

- Documented build instructions for common compilers and MPI implementations.
- CMake or equivalent configuration that makes backend choices explicit.
- Container or module-based recipes where appropriate for cluster environments.
- Job scripts for common schedulers such as Slurm, clearly marked as templates.
- Regression tests that can run at small scale before a full job is submitted.
- Profiling instructions for CPU, memory, MPI communication, and I/O.
- Checkpoint/restart workflows suitable for queue-limited systems.
- Output-management recommendations for large datasets.
- Hardware notes for single-node, multi-node CPU, and future GPU-enabled runs.

These requirements are not peripheral. A solver that works only on one developer’s machine is not yet a community scientific instrument. Moving from v0.6.8 toward v1.0 will require packaging, testing, and documentation work that is less visible than new physics features but equally important.

### Access pathways for independent researchers

The roadmap includes exploring pathways that could connect independent researchers, students, and small teams with institutional or shared computing resources. This may involve partnerships, educational allocations, benchmark-friendly cluster accounts, cloud credits, or collaborations with groups that already operate HPC systems.

This should be described carefully: the project can plan for these pathways, but access to institutional HPC is governed by policies, resource committees, security requirements, and funding constraints. GRANITE cannot assume access until concrete agreements exist. The practical near-term task is to make the software easy for an HPC administrator or research group to evaluate: clear build steps, reproducible benchmarks, contained dependencies, and predictable resource behavior.

### Role of cluster administrators and HPC engineers

Cluster administrators and HPC engineers are essential contributors for this pillar. They can identify deployment friction that physicists and application developers often miss: filesystem pressure, MPI launch behavior, module conflicts, scheduler limits, job isolation, security policies, and monitoring requirements.

The project should explicitly invite this expertise. A successful v1.0 deployment path should include feedback from people who operate shared systems, not only from people who write numerical kernels.

### Practical milestones

A useful sequence of milestones for this pillar is:

1. A minimal local build and reduced benchmark that runs on a workstation.
2. A documented multi-process MPI test on a single node.
3. A small multi-node benchmark with reproducible input YAML and expected diagnostics.
4. Checkpoint/restart validation under scheduler time limits.
5. Performance profiling and scaling plots for selected benchmarks.
6. Optional container or environment recipes for selected clusters.
7. A contributor-maintained catalog of successful deployments, including hardware, compiler, MPI, and known caveats.

This approach keeps HPC accessibility grounded in reproducible operational evidence rather than general aspiration.

---

## Pillar V: Hardware-Agnostic Execution and Future Photonic Computing Readiness

The long-term performance roadmap for GRANITE includes hardware abstraction that could support CPU, GPU, and future accelerator backends. Photonic or optical computing concepts are included as a future-readiness direction, not as a present capability. The immediate engineering value of this pillar is the separation of mathematical operators from execution policy; that separation is useful even if photonic hardware is never used.

The motivation is practical. High-resolution NR and GRMHD simulations place pressure on memory bandwidth, interconnect latency, synchronization, and energy consumption. CCZ4 and GRMHD evolution involve many coupled fields per grid point, stencil operations, ghost-zone exchange, and repeated reductions or diagnostics. These workloads can become limited by data movement as much as floating-point arithmetic.

### Hardware-agnostic policy-based design

The planned architectural direction is to decouple the mathematical specification of an operation from the backend that executes it. A finite-difference stencil, tensor operation, reduction, or communication step should be represented in a way that can be mapped to different execution policies.

On current systems, those policies may target:

- Standard CPU execution.
- SIMD-aligned CPU kernels.
- MPI-distributed memory execution.
- Future GPU kernels.
- Hybrid CPU/GPU decomposition.
- Experimental accelerator interfaces where appropriate.

A policy-based design can reduce backend-specific duplication, but it introduces its own risks: abstraction overhead, more complicated debugging, template complexity, build-time costs, and difficulty in proving that different backends compute numerically equivalent results. This pillar therefore depends on strong testing, backend comparison, and performance measurement.

### Data abstraction and mixed precision

The roadmap includes abstracting data types, memory layouts, and allocation strategies through structures such as:

`DenseTensor3D<ComputeType, StorageType, Allocator>`

The purpose is to make precision and storage policy explicit. Some parts of a simulation may require FP64 for stability, constraint behavior, and reliable convergence. Other parts, especially preconditioning, approximate linear algebra, reduced diagnostics, or visualization preprocessing, may tolerate lower precision if error is controlled and verified.

Mixed-precision iterative refinement is the proposed strategy for exploring this boundary. In such a workflow, a lower-precision backend may generate an approximate solution or preconditioner, while FP64 computation performs residual correction and final accuracy control. For example, elliptic constraint-solving components, such as those related to Bowen-York initial-data workflows, could be investigated as candidates for this kind of decomposition.

This is a research and engineering target, not a claim of immediate correctness. It requires numerical experiments showing where lower precision is safe, where it fails, and how errors propagate into constraints, waveforms, and conserved quantities. Each candidate kernel needs benchmark comparison against a full FP64 baseline.

### Photonic and optical computing readiness

Future optical or photonic processors may be relevant for selected dense linear-algebra operations or interconnect-heavy workloads. Some proposed systems use optical matrix-vector multiplication or optical interconnect fabrics, which could in principle reduce latency or energy cost for specific communication or multiply-accumulate patterns.

GRANITE’s practical design response is not to assume that photonics will solve NR workloads wholesale. Instead, the code should avoid unnecessary coupling to a single execution model. If future photonic hardware exposes useful matrix-vector, preconditioning, or interconnect primitives, GRANITE should have a clean enough operator layer to experiment with them.

Potential exploratory targets include:

- Mapping selected finite-difference operators to matrix or structured-matrix representations where that is numerically and computationally sensible.
- Investigating Toeplitz or stencil-equivalent representations for accelerator dispatch.
- Separating low-precision preconditioning from FP64 correction.
- Abstracting ghost-zone synchronization behind an `ICommunicator` interface.
- Evaluating whether optical interconnect concepts could reduce boundary-exchange latency for AMR blocks on future systems.

The phrase “photonic readiness” should therefore mean architectural preparedness and experimental interfaces, not present-day dependence on unavailable hardware. The required evidence will be benchmark data, numerical-error analysis, and backend comparisons once suitable hardware or simulators are accessible.

### Operator dispatch and communication abstraction

The mathematical formulation—the “what”—should be represented separately from the execution policy—the “how.” For example, a fourth-order finite-difference operation should be defined as a mathematical operator with known stencil coefficients, boundary requirements, and precision expectations. On x86_64 hardware, it may compile to SIMD-friendly loops. On a GPU, it may map to tiled kernels. On a future accelerator, it may dispatch through a different representation if the data movement and accuracy tradeoffs justify it.

Similarly, ghost-zone synchronization should be abstracted behind an interface such as `ICommunicator`. Today that interface would likely wrap MPI behavior. In the future, it could support alternative transport mechanisms or optimized interconnect paths. The main requirement is that communication semantics remain testable: boundary data must arrive correctly, at the right synchronization points, without hidden assumptions about ordering or precision.

### Development dependencies

This pillar depends on contributors with experience in numerical methods, C++ performance engineering, GPU programming, distributed memory systems, and accelerator benchmarking. It also depends on conservative validation. A faster backend that changes the convergence behavior or constraint violation profile is not an acceptable replacement for a slower correct backend.

The near-term value is cleaner architecture for CPU/GPU portability. The long-term value is the possibility of testing future accelerator models without rewriting the scientific core.

---

## Roadmap to v1.0 and Beyond

The present foundation is identified as v0.6.8 in the project vision. The roadmap from that state to v1.0 should be treated as a sequence of verifiable milestones rather than a single release claim.

A practical roadmap is:

1. Stabilize the configuration schema and define the first complete YAML scenario format.
2. Convert existing benchmark examples into versioned, documented YAML scenarios.
3. Add parser validation, schema tests, and clear diagnostics for invalid configurations.
4. Establish reduced validation benchmarks with expected outputs and tolerances.
5. Improve build and deployment documentation for local, MPI, and small-cluster execution.
6. Define the telemetry schema for VORTEX and implement low-bandwidth runtime diagnostics.
7. Add WebGL-based browser views for trajectories, horizons, wave diagnostics, and constraint trends.
8. Create the initial community challenge board with contribution labels and validation status.
9. Begin GPU-porting work where profiling shows a clear performance case.
10. Introduce hardware-abstraction interfaces only where they are justified by maintainability and tests.
11. Explore mixed-precision and accelerator-readiness experiments after FP64 baselines are established.
12. Prepare a v1.0 release only when documentation, reproducibility, tests, and contributor workflows are strong enough for external users.

This roadmap is intentionally staged. It keeps the most testable work near the front: configuration, benchmarks, build reproducibility, and telemetry. More speculative items, such as photonic computing readiness, remain part of the long-term architecture but should not be allowed to distract from correctness, stability, and usability.

---

## Closing Statement

GRANITE will remain an open-source project under the GPL-3.0 license. The immediate goal is to turn the existing v0.6.8 foundation into a reproducible, documented, community-usable research code for NR/GRMHD experimentation and education. The longer-term goal is to build a modular architecture that can support YAML-defined scenarios, runtime WebGL telemetry through VORTEX, structured community challenges, realistic HPC deployment, GPU acceleration, and future accelerator experiments.

This is not a completed system being declared finished. It is a technical program with clear dependencies and nontrivial engineering risk. The work ahead includes numerical validation, software architecture, HPC deployment, documentation, visualization, governance, and contributor coordination.

GRANITE will be strongest if it becomes a shared engineering and scientific effort. Physicists can help define and validate scenarios. HPC engineers can make the code deployable and measurable. GPU and systems programmers can improve performance paths. Interface designers can make telemetry understandable. Students and independent researchers can run benchmarks, report failures, improve documentation, and help turn examples into reproducible workflows.

The project’s next meaningful step is to make each planned capability testable: one schema, one benchmark, one telemetry stream, one deployment recipe, one community challenge at a time.
