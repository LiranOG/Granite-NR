# Genesis & Architecture — The Developer's Navigation Guide

### How to Read, Navigate, and Contribute to the GRANITE Engine — And Why the Theory Came First

---

<div align="center">

*"The analytical work is the source of truth.*
*The C++ code is the validation engine.*
*If you find a discrepancy, the derivation wins — until it doesn't.*
*And when it doesn't, you've discovered new physics."*

— The GRANITE Design Covenant

</div>

---

## Purpose of This Document

This guide is written for the developer, physicist, or researcher approaching the GRANITE engine for the first time. It provides the navigational logic — the precise order in which the codebase and its theoretical foundations should be read, understood, and extended — and it enforces a single, non-negotiable architectural constraint:

> **The analytical derivations in the [GRANITE Astrophysics Suite](https://github.com/LiranOG/GRANITE-Astrophysics-Suite) are the source of truth. The C++ code in this repository is the validation engine. The kinematic engines in the Suite are the experimental apparatus that bridges them.**

This hierarchy is not a suggestion. It is the load-bearing structure of the entire project. Every design decision, every module boundary, every benchmark threshold in the production engine traces its provenance to a specific equation in the theoretical layer. To modify code without understanding the derivation it implements is to operate without a map in territory that will not forgive navigation errors.

---

## Part I — The Architect's Journey: Where to Begin

### 1.1 Start With the Questions, Not the Code

The most common mistake a new contributor can make is to open `src/spacetime/ccz4.cpp` and begin reading. That file implements a CCZ4 spacetime evolution scheme — but without understanding *why* CCZ4 was chosen over BSSN, *why* the constraint damping parameters are set to $\kappa_1 = 0.02/M_{\text{ADM}}$, and *why* the evolution must remain stable over $O(10^4\,M)$ of coordinate time, the code is opaque.

**Begin here instead — in the GRANITE Astrophysics Suite:**

1. **[THE GENESIS OF GRANITE](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/03_GRANITE_Engine/THE_GENESIS_OF_GRANITE.md)** — Read this first. In its entirety. It is the founding manifesto: the Frightening Questions that launched the project, the analytical era that solved them, and the bridge from solved mathematics to required machine.

2. **The Frightening Questions** (Genesis, Part I): Understand the two research scenarios:
   - **The Septad (ENAE-7):** *What happens when five $10^8\,M_\odot$ SMBHs in a symmetric pentagon undergo simultaneous radial coalescence?*
   - **The Octad (ENAE-8):** *What happens when 3 neutron stars, 2 Wolf-Rayet supergiants, and 5 R136a1-class stars converge on the same galactic nucleus?*

3. **The $10^{53}$ Erg Ceiling** (Genesis, Part I): Understand why the quasar energy ceiling ($\sim10^{53}$ erg) was the intellectual starting point, and how the B5_STAR scenario shattered it by eleven orders of magnitude.

Only once you understand *what* the project is trying to compute should you proceed to *how* the computation was structured.

### 1.2 Read the Theory in Dependency Order

The five analytical frameworks in the [GRANITE Astrophysics Suite](https://github.com/LiranOG/GRANITE-Astrophysics-Suite) form a strict dependency chain. Read them in this order — any other order will leave critical definitions undefined:

```
Step 1:  NRCF   →  Understand the geometric shape factor s_N
Step 2:  PRISM  →  Understand the coherent GW radiation budget
Step 3:  SYNAPSE →  Understand the full merger cascade with running ε_k
Step 4:  AUE    →  Understand the systematic uncertainty audit
Step 5:  NEXUS  →  Understand the 4-phase multi-species cascade
```

| Step | Document | What You Learn | Time Estimate |
|------|----------|---------------|---------------|
| 1 | [NRCF — Radial Collapse Framework](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/ENAE_7_Septad/NRCF_Radial_Collapse_Framework.md) | How $N$-body symmetric polygons reduce to effective two-body Kepler problems via $s_N$ | 30 min |
| 2 | [PRISM — Polyhedral Radial Infall](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/ENAE_7_Septad/PRISM_Polyhedral_Radial_Infall.md) | Why GW power scales as $N^2$ (coherent superradiance), the GW energy formula | 45 min |
| 3 | [SYNAPSE — Phase-Space Estimator](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/ENAE_7_Septad/SYNAPSE_Phase_Space_Estimator.md) | The merger cascade tree, running efficiency $\varepsilon_k$, spin recursion, slim-disk EM correction | 60 min |
| 4 | [AUE — Astrophysical Ultimate Estimator](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/ENAE_7_Septad/AUE_Astrophysical_Ultimate_Estimator.md) | Where the analytical models break down, what physics they omit, and what machine is required | 45 min |
| 5 | [NEXUS — Neutron-star Extreme Scenario](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/ENAE_8_Octad/NEXUS_Neutron_Star_Extreme_Scenario.md) | The 4-phase tri-species cascade, 6 novel constructs, 6 falsifiable predictions | 60 min |

**Critical supplements in the Suite:**
- [Septad vs. Octad — Divergent Physics](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/Comparative_Analysis/ENAE_Divergent_Physics_Septad_vs_Octad.md) — The Compactness Barrier as organizing principle.
- [GRANITE Engine Specification](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/Engine_Specification/GRANITE_Engine_Specification.md) — The formal bridge between the AUE's requirements and this engine's architecture.

### 1.3 Explore the Kinematic Engines

After reading the theory, open the kinematic engines from the Suite's [`02_Kinematic_Engines/`](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/tree/main/02_Kinematic_Engines) in a web browser. No installation required — every engine is a self-contained HTML file with embedded JavaScript.

**Recommended exploration order:**

1. **`ENAE_7_Septad/PRISM_interactive.html`** — Adjust $N$, $M_{\text{BH}}$, and $R_0$ with sliders. Watch the GW energy, EM burst, and merger cascade update in real time. Verify that the numbers match the PRISM manuscript.

2. **`ENAE_7_Septad/SYNAPSE_Framework.html`** — Step through the merger cascade. Watch spin accumulate across merger steps. Confirm the running efficiency amplification.

3. **`ENAE_8_Octad/VORTEX/VORTEX_ETERNITY.html`** — The fully operational WebGL 3D engine. Configure a multi-body scenario, run the simulation, observe gravitational lensing, hear gravitational wave sonification.

4. **`ENAE_8_Octad/SECOND_HALF/NEXUS.html`** — Visualize the 4-phase NEXUS cascade with energy budget breakdowns.

These engines are not demonstrations. They are the **computational laboratory** — the experimental apparatus that validated every analytical prediction before this C++ engine existed.

---

## Part II — From Solved Physics to Required Machine

### 2.1 The Strong-Field Frontier

The analytical stack owns everything above $r \sim 10\,R_s$. Below this radius:
- The post-Newtonian expansion diverges irrecoverably
- The quadrupole approximation loses its mathematical basis
- The spacetime metric participates dynamically in radiation emission
- Apparent-horizon topology transitions cannot be described in closed form

This boundary is exact — derived from the analytical models' documented failure modes. GRANITE's domain begins here. The analytical models hand off at $10\,R_s$; this engine takes over.

### 2.2 The AUE → Engine Specification Pipeline

The AUE's systematic uncertainty audit identified four ranked failure modes. Each failure mode maps directly to a module in this engine:

```
AUE Failure Mode                    →  GRANITE Engine Module
────────────────────────────────────────────────────────────
GR divergence below 10 R_s          →  CCZ4 spacetime evolution
GW radiation backreaction            →  Coupled GW extraction
Radiation hydro (10⁶× EM error)     →  M1 radiation transport  
Microphysics (e⁺e⁻ → νν̄)          →  Neutrino leakage scheme
Dynamic range (10⁸ spatial)          →  25+ level Berger-Oliger AMR
```

**There is no module in this engine whose existence cannot be justified by pointing to a specific equation in the [Astrophysics Suite](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/tree/main/01_Theoretical_Limit).** This is not a rhetorical claim. It is a verifiable, auditable, falsifiable fact.

### 2.3 Tracing a Module to Its Origin

For any module in the GRANITE engine, you should be able to trace a direct provenance chain back to the Suite:

**Example: CCZ4 constraint damping parameter $\kappa_1 = 0.02/M_{\text{ADM}}$**

1. **[SYNAPSE](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/ENAE_7_Septad/SYNAPSE_Phase_Space_Estimator.md)** (Section C) derives the merger cascade timescale: $O(10^3\text{–}10^4\,M)$
2. **[AUE](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/ENAE_7_Septad/AUE_Astrophysical_Ultimate_Estimator.md)** (Section D.1) identifies that BSSN cannot maintain constraint stability over this duration
3. **[Engine Specification](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/Engine_Specification/GRANITE_Engine_Specification.md)** (Section 1.3) specifies CCZ4 with $\kappa_1 = 0.02/M_{\text{ADM}}$, calculating the damping timescale $\tau_{\text{damp}} = 1/(\alpha\kappa_1)$
4. **`src/spacetime/ccz4.cpp`** ← implements the evolution equation with this exact parameter

**Example: Valencia GRMHD conservative formulation**

1. **[AUE](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/ENAE_7_Septad/AUE_Astrophysical_Ultimate_Estimator.md)** (Section D.3) proves the stellar disruption physics requires exact conservation on curved spacetime
2. **AUE** (Section D.3) quantifies the naive EM estimate as physically unrealizable by $10^6\times$
3. **[Engine Specification](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/Engine_Specification/GRANITE_Engine_Specification.md)** (Section 2) specifies the Valencia formulation with HRSC
4. **`include/granite/grmhd.hpp`** ← implements $\partial_t\mathbf{U} + \partial_i\mathbf{F}^i = \mathbf{S}(\mathbf{U}, g_{\mu\nu})$

---

## Part III — Repository Map

### 3.1 This Repository — The Engine

```
Granite/                            ← You are here
├── src/
│   ├── spacetime/                  ← CCZ4 evolution, gauge, moving punctures
│   ├── matter/                     ← GRMHD + HRSC, EOS tables, con2prim
│   ├── radiation/                  ← Grey M1, neutrino leakage
│   ├── amr/                        ← Block-structured AMR + subcycling
│   ├── diagnostics/                ← ψ₄ extraction, AH finder, constraint monitor
│   └── io/                         ← HDF5, checkpointing
├── include/granite/                ← Public headers
├── tests/                          ← Benchmark suite (92 tests, 100% pass)
├── docs/
│   └── GENESIS_AND_ARCHITECTURE.md ← This document
├── README.md                       ← Project overview
└── CMakeLists.txt                  ← Build system
```

### 3.2 The Companion Repository — The Theory

```
GRANITE-Astrophysics-Suite/         ← The Genesis Archive
│
├── 01_Theoretical_Limit/           ← FIRST: The mathematics was solved
│   ├── ENAE_7_Septad/              ← Septad (vacuum GR): NRCF, PRISM, SYNAPSE, AUE
│   ├── ENAE_8_Octad/               ← Octad (hydrodynamic): NEXUS
│   ├── Comparative_Analysis/       ← Compactness Barrier: Septad vs. Octad
│   ├── Engine_Specification/       ← The machine the math demanded
│   └── archive/                    ← Raw historical sources (PDF, DOCX, 7z)
│
├── 02_Kinematic_Engines/           ← SECOND: The physics was explored
│   ├── ENAE_7_Septad/              ← PRISM, SYNAPSE, AUE, GRANITE Spec engines
│   ├── ENAE_8_Octad/
│   │   ├── ECLIPSE/                ← Progressive N-body integrators (×3)
│   │   ├── VORTEX/                 ← 3D WebGL engines (×7, incl. ETERNITY)
│   │   ├── FIRST_HALF/             ← Pre-merger dynamics engines
│   │   └── SECOND_HALF/            ← Post-merger cascade engines
│   └── Standalone/                 ← Cross-scenario comparative analysis
│
├── 03_GRANITE_Engine/              ← THIRD: The reason this repo exists
│   ├── THE_GENESIS_OF_GRANITE.md   ← The founding document
│   └── README.md                   ← Engine directory overview
│
├── WIKI_OVERVIEW.md                ← Comprehensive project wiki
├── README.md                       ← Suite overview
└── LICENSE.md                      ← Dual licensing (GPL-3.0 / CC BY-SA 4.0)
```

### 3.3 Cross-Reference Map: Theory ↔ Engine

| Analytical Framework | Central Equation | Engine Module | Engine Location |
|---------------------|-----------------|---------------|-----------------|
| **[NRCF](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/ENAE_7_Septad/NRCF_Radial_Collapse_Framework.md)** | $s_N = \frac{1}{4}\sum\csc(\pi k/N)$ | Initial data (polygon BH placement) | `src/initial_data/` |
| **[PRISM](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/ENAE_7_Septad/PRISM_Polyhedral_Radial_Infall.md)** | $E_{\text{GW}} = \frac{1}{210}\frac{M_{\text{ring}}^2 M_{\text{eff}}^{5/2}}{M_{\text{tot}}^{7/2}}c^2$ | GW extraction ($\psi_4$ diagnostic) | `src/diagnostics/` |
| **[SYNAPSE](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/ENAE_7_Septad/SYNAPSE_Phase_Space_Estimator.md)** | $\varepsilon_k = \varepsilon_0 \cdot 4\eta_k \cdot f_{\text{spin}}$ | Benchmark validation targets | `tests/` |
| **[AUE](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/ENAE_7_Septad/AUE_Astrophysical_Ultimate_Estimator.md)** | Systematic failure audit | All physics modules | `src/spacetime/`, `src/matter/`, `src/radiation/` |
| **[NEXUS](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/ENAE_8_Octad/NEXUS_Neutron_Star_Extreme_Scenario.md)** | 4-phase cascade | Multi-physics coupling | `src/matter/` + `src/radiation/` |
| **[Engine Spec](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/Engine_Specification/GRANITE_Engine_Specification.md)** | CCZ4 + GRMHD + M1 + AMR | Complete architecture | Entire `src/` tree |

---

## Part IV — The Math-First Dogma: Rules for Contributors

### 4.1 The Golden Rules

1. **No new module without a derivation.** If you need physics that isn't in the [Astrophysics Suite](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/tree/main/01_Theoretical_Limit), derive it first — in closed form, with documented assumptions, limiting-case recovery, and quantified uncertainty. Then write the code.

2. **No test without an analytical target.** Every benchmark test must be written against a known analytical prediction. "Looks reasonable" is not a pass criterion. "Matches the derivation within stated uncertainty bounds" is.

3. **No architecture change without traceability.** If you propose a new numerical scheme, a different gauge condition, an alternative reconstruction method — you must trace the necessity to a specific physical scenario and a specific equation that demands it.

4. **No code before the theory is complete.** This is the founding constraint. It was enforced from the first equation to the last benchmark. If you are extending GRANITE to a new scenario, the analytical work must precede the implementation.

### 4.2 The Verification Protocol

When the GRANITE engine produces numerical output, verification proceeds as follows:

1. **Identify the analytical prediction.** For the scenario being simulated, what does the [theoretical framework](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/tree/main/01_Theoretical_Limit) predict? At what precision? Under what assumptions?

2. **Identify the strong-field frontier.** Where does the analytical prediction break down? (Typically $r \sim 10\,R_s$.) Above this radius, the engine's output must match the analytical prediction. Below, the engine is producing new physics.

3. **Compare quantitatively.** Not qualitatively. Not "similar order of magnitude." The comparison must be numerical, with stated error bars inherited from both the analytical uncertainty (AUE) and the numerical truncation error.

4. **Document divergence.** If the engine's output deviates from the analytical prediction in a regime where the analytical model should be valid, this is a **bug** — not a "feature" or "numerical effect." Debug until convergence is achieved, or identify the specific analytical assumption that the simulation has falsified.

---

## Part V — The VORTEX ↔ GRANITE Convergence

The kinematic engines in the Suite's [`02_Kinematic_Engines/`](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/tree/main/02_Kinematic_Engines) were not side projects. They were Phase 2 of a three-phase architecture — and their convergence with the GRANITE engine was always the plan.

### 5.1 The Convergence Architecture

```
                    ┌──────────────────────────────────────────┐
                    │     Astrophysics Suite                    │
                    │     01_Theoretical_Limit/                │
                    │     (Analytical Predictions)             │
                    └─────────────┬────────────────────────────┘
                                  │
                    ┌─────────────▼────────────────────────────┐
      ┌─────────────┤     Engine Specification                 │
      │             │     (Machine Requirements)               │
      │             └─────────────┬────────────────────────────┘
      │                           │
      ▼                           ▼
┌──────────────┐    ┌──────────────────────────────────────────┐
│ VORTEX       │    │ GRANITE Engine                           │
│ ETERNITY     │◄───┤ (This Repository)                        │
│ (WebGL       │    │ C++20, CCZ4+GRMHD+M1+AMR                │
│  Viewer)     │    │ Produces HDF5 checkpoint data            │
└──────────────┘    └──────────────────────────────────────────┘
     ▲
     │ HDF5 → kinematic data stream
     │
     └── Real-time 3D playback in any web browser
```

In the v1.0 architecture, GRANITE's HDF5 checkpoint outputs will be distilled into kinematic data streams and ingested by [VORTEX ETERNITY](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/tree/main/02_Kinematic_Engines/ENAE_8_Octad/VORTEX) as a high-fidelity 3D playback viewer — enabling real-time interactive exploration of full general-relativistic numerical relativity simulation data in any web browser, on any machine, with no installation required.

### 5.2 Historical Role

The kinematic engines validated the analytical predictions through a parallel, complementary technology. They implemented:
- Real-time NRCF equations of motion
- Interactive SYNAPSE cascade computation with live parameter sweeps
- NEXUS 4-phase cascade visualization with energy budget breakdowns
- 2.5PN radiation reaction at 60 fps in a standard web browser

They are preserved verbatim in the Suite because they are the **computational laboratory notebooks** — the record of how physics was explored before this engine existed to validate it formally.

---

## Part VI — Connecting the Two Repositories

### 6.1 Repository Topology

```
github.com/LiranOG/Granite                       (This Repository)
──────────────────────────────────────────────────
Contains:  C++20 production engine (CCZ4, GRMHD, M1, AMR)
Role:      Numerical validation engine — implements the physics derived in the Suite
Version:   v0.6.7 · 92 tests · 100% pass rate
License:   GPL-3.0

github.com/LiranOG/GRANITE-Astrophysics-Suite    (The Genesis Archive)
──────────────────────────────────────────────────
Contains:  Theory (01), Kinematic engines (02), Genesis Archive (03)
Role:      Source of truth for all physics, derivations, and design rationale
License:   CC BY-SA 4.0 (academic content) + GPL-3.0 (source code)
```

### 6.2 The Information Flow

The information flow is **strictly unidirectional**:

```
Astrophysics Suite: Theory (NRCF → PRISM → SYNAPSE → AUE → NEXUS)
    │
    ▼ specifies requirements
Astrophysics Suite: Engine Specification
    │
    ▼ authorizes construction
This Repository: GRANITE C++ Engine
    │
    ▼ produces numerics
Comparison against analytical predictions
    │
    ▼ validates or falsifies
Physics conclusions
```

The theory never adapts to the code. The code adapts to the theory. If the code disagrees with the theory in a regime where the theory should be valid, the code is wrong. If the code disagrees with the theory in the strong-field regime where the theory explicitly breaks down, the code has produced **new physics** — and a new analytical framework must be written to explain it.

---

## Part VII — Quick Reference for Common Tasks

### "I want to understand the project."
→ Read [THE GENESIS OF GRANITE](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/03_GRANITE_Engine/THE_GENESIS_OF_GRANITE.md), then the Suite's [WIKI_OVERVIEW.md](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/WIKI_OVERVIEW.md).

### "I want to understand a specific engine module."
→ Trace from the module to its entry in the [Engine Specification](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/Engine_Specification/GRANITE_Engine_Specification.md), then to the AUE failure mode that demanded it.

### "I want to verify the analytical predictions interactively."
→ Clone the [Astrophysics Suite](https://github.com/LiranOG/GRANITE-Astrophysics-Suite) and open the engines in `02_Kinematic_Engines/` in a browser. Start with PRISM, then SYNAPSE.

### "I want to run the C++ engine."
→ Follow the build instructions in this repository's [README.md](../README.md). Run the benchmark suite. Compare against the analytical targets in [SYNAPSE](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/01_Theoretical_Limit/ENAE_7_Septad/SYNAPSE_Phase_Space_Estimator.md).

### "I want to add a new physics module."
→ First: derive the physics in closed form. Document assumptions, limiting cases, and quantified uncertainty. Add the manuscript to the Suite's `01_Theoretical_Limit/`. Prototype the physics in an interactive engine and add it to the Suite's `02_Kinematic_Engines/`. Only then implement in C++ in this repository.

### "I want to cite this work."
→ Use the BibTeX entry in the [Suite README](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/README.md) or this repository's README. DOI: [10.5281/zenodo.19502265](https://doi.org/10.5281/zenodo.19502265).

---

<div align="center">

*The code is here. The theory is in the [GRANITE Astrophysics Suite](https://github.com/LiranOG/GRANITE-Astrophysics-Suite).*
*The reason both exist is in [THE GENESIS OF GRANITE](https://github.com/LiranOG/GRANITE-Astrophysics-Suite/blob/main/03_GRANITE_Engine/THE_GENESIS_OF_GRANITE.md).*

<br>

*GRANITE — General-Relativistic Adaptive N-body Integrated Tool for Extreme Astrophysics*
*Genesis & Architecture Guide — April 2026*

**Founder & Lead Architect: LiranOG**

[![ORCID](https://img.shields.io/badge/ORCID-0009--0008--8035--1308-A6CE39?logo=orcid&logoColor=white)](https://orcid.org/0009-0008-8035-1308)
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.19502265.svg)](https://doi.org/10.5281/zenodo.19502265)

</div>
