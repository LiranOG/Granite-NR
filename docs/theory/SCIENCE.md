# GRANITE — Science & Physics Reference

**Version:** v0.6.7 (current) | **April 27, 2026**

> This document explains the physical and mathematical foundations of GRANITE: what it simulates, why it matters, and which governing equations it implements. For implementation details, see [`DEVELOPER_GUIDE.md`](../developer_guide/DEVELOPER_GUIDE.md).

---

## Table of Contents

1. [The Scientific Problem](#1-the-scientific-problem)
2. [Why Existing Codes Are Not Enough](#2-why-existing-codes-are-not-enough)
3. [The B5\_star Scenario — Flagship Target](#3-the-b5_star-scenario--flagship-target)
4. [Governing Equations](#4-governing-equations)
5. [Numerical Approach](#5-numerical-approach)
6. [Multi-Messenger Observables](#6-multi-messenger-observables)
7. [Physical Scales & Unit System](#7-physical-scales--unit-system)
8. [References](#8-references)

---

## 1. The Scientific Problem

### 1.1 Extreme Multi-Body Astrophysics

The most energetic events in the known universe involve the coalescence of compact objects — black holes and neutron stars — under the influence of general relativity. The gravitational waves they emit are detectable by LIGO, Virgo, and LISA; the electromagnetic and neutrino counterparts are observed by every major telescope network on Earth and in space.

The physics of these events is governed by four coupled systems:

1. **The spacetime metric** — the dynamical curved geometry of space and time, governed by the Einstein field equations
2. **The matter fields** — gas, plasma, and nuclear matter obeying the equations of relativistic magnetohydrodynamics
3. **The electromagnetic field** — large-scale magnetic fields embedded in the accretion disc and jet
4. **The radiation field** — neutrinos and photons carrying away energy and lepton number

In the simplest case — a binary black hole merger — items 2, 3, and 4 are absent or negligible, and the problem reduces to pure spacetime dynamics. This is the regime where existing codes like SpECTRE, GRChombo, and the Einstein Toolkit have achieved extraordinary precision.

**GRANITE's scientific domain begins where these codes reach their limits:** events involving multiple compact objects *simultaneously*, in the presence of magnetized matter, radiation, and neutrino physics.

### 1.2 Supermassive Black Hole Mergers

In the centers of massive galaxies, supermassive black holes (SMBHs) with masses 10⁶–10¹⁰ M☉ undergo hierarchical mergers driven by dynamical friction, stellar loss-cone scattering, and gravitational wave emission. The final parsec problem — how SMBHs close the last parsec of separation — remains one of the most important open questions in extragalactic astrophysics.

In dense galactic nuclei and galaxy merger remnants, the coalescence may involve **more than two SMBHs simultaneously**, interacting with dense stellar clusters, accretion discs, and each other. This is precisely the regime no existing code handles.

---

## 2. Why Existing Codes Are Not Enough

### 2.1 The Capability Gap

| Code | Strength | Key Gap |
|---|---|---|
| **Einstein Toolkit** | Mature, modular, community-tested | Modular thorn system creates tight coupling barriers for multi-physics; limited to binary mergers in practice |
| **GRChombo** | Clean C++17, AMR, BBH/BNS | No M1 radiation, no neutrino leakage, no multi-BH N > 2 |
| **SpECTRE** | Spectral methods, high accuracy | GRMHD partial, no radiation, massive engineering overhead |
| **AthenaK** | Excellent GRMHD, GPU-native | No CCZ4 (metric is fixed or GRFFE only), no dynamic spacetime |
| **GRANITE** | All of the above in one framework | Under active development — see Known Limitations |

### 2.2 The Unification Problem

The fundamental difficulty is not implementing any single piece of physics — all of CCZ4, GRMHD, and M1 radiation have been implemented separately in existing codes. The difficulty is **coupling them correctly and efficiently** in a single evolution loop, with:

- A shared dynamical metric updated by CCZ4 that the GRMHD solver reads at every RK3 stage
- Source terms from radiation coupling into the GRMHD energy equation
- Neutrino leakage from GRMHD coupling into the radiation distribution function
- AMR refinement criteria that simultaneously track puncture motion and matter density gradients

This is the engineering and physics challenge GRANITE is built to solve.

---

## 3. The B5\_star Scenario — Flagship Target

The **B5\_star benchmark** is GRANITE's primary scientific motivation. It represents a scenario with no existing code capable of simulating it at full physics fidelity.

### 3.1 Configuration

- **5 supermassive black holes**, each 10⁸ M☉, arranged in a regular pentagon at 1 pc radius
- **2 ultra-massive stars**, each ~4300 M☉, positioned at the center
- **Evolution time:** 10⁵ years
- **Full physics:** CCZ4 spacetime + GRMHD + M1 radiation + neutrino leakage + dynamic AMR (12 levels)

### 3.2 Expected Physical Sequence

The B5\_star evolution is expected to proceed through the following phases:

**Phase I — Stellar disruption (t ~ 10³–10⁴ yr):** The central stars are tidally disrupted by the SMBH quintuple. Tidal disruption events (TDEs) produce luminous flares observable in X-ray and UV. The disrupted matter forms accretion structures around the nearest SMBHs.

**Phase II — SMBH inspiral (t ~ 10⁴–10⁵ yr):** Gravitational radiation and dynamical friction drive the SMBHs inward. The mergers occur sequentially or in rapid succession depending on the mass ratios and orbital parameters. Each merger produces a burst of gravitational waves in the PTA / LISA frequency band.

**Phase III — Remnant evolution:** The final merged SMBH accretes residual matter and launches MHD jets. The total mass radiated in gravitational waves is ΔM ~ η M_total c² with η ~ 0.05 (numerical relativity estimate for equal-mass mergers).

### 3.3 Multi-Messenger Signatures

| Messenger | Observable | Frequency / Band | Detector |
|---|---|---|---|
| Gravitational waves | Inspiral chirp, merger burst | 10⁻⁹–10⁻⁷ Hz | PTA (NANOGrav, EPTA, PPTA) |
| Gravitational waves | Ringdown | 10⁻⁴–10⁻³ Hz | LISA |
| X-ray emission | TDE flares | 0.1–10 keV | Chandra, XMM-Newton, eROSITA |
| Radio emission | MHD jet | GHz | VLBI, SKA |
| Neutrinos | Accretion disc | MeV | IceCube (if galactic) |

### 3.4 Computational Requirements

| Parameter | Value |
|---|---|
| AMR levels | 12 |
| Finest cell size | ~10⁻⁴ pc (resolves SMBH horizon) |
| Coarsest cell size | ~1 pc (encloses full system) |
| Grid cells (peak) | ~10¹⁰ (across all AMR levels) |
| RAM (estimated) | ~2 TB |
| CPU-hours (estimated) | ~5 × 10⁶ |
| GPU-hours (estimated, H100) | ~5 × 10⁵ |

These requirements place B5\_star firmly in the Tier-0 supercomputer category. The current v0.6.7 development work on desktop hardware is building and validating the physics and numerics at 128³ resolution; the full production run will require institutional HPC allocation.

---

## 4. Governing Equations

### 4.1 Spacetime: CCZ4 Formulation

GRANITE evolves the Einstein field equations

$$G_{\mu\nu} = 8\pi T_{\mu\nu}$$

in the CCZ4 (Conformal and Covariant Z4) formulation (Alic et al. 2012). The CCZ4 system promotes the algebraic constraint $Z_\mu = 0$ to a dynamical field, enabling exponential damping of constraint violations.

The conformal decomposition introduces:

$$\tilde{\gamma}_{ij} = \chi \, \gamma_{ij}, \qquad \chi = \left(\det \gamma_{ij}\right)^{-1/3}$$

The full set of 22 evolution variables is:

| Variable | Symbol | Role |
|---|---|---|
| Conformal factor | χ | Encodes volume element |
| Conformal metric | γ̃ᵢⱼ (6 components) | Spatial geometry |
| Extrinsic curvature trace | K | Expansion of spatial slices |
| Traceless conf. extrinsic curv. | Ãᵢⱼ (6 components) | Shear of spatial slices |
| Conf. Christoffel functions | Γ̃ⁱ (3 components) | Connections |
| Lapse | α | Time dilation |
| Shift vector | βⁱ (3 components) | Frame dragging |
| CCZ4 constraint variable | Θ | Algebraic constraint |

The CCZ4 evolution equations for the key variables:

$$\partial_t \chi = \frac{2}{3}\chi\left(\alpha K - \partial_i \beta^i\right) + \beta^i \partial_i \chi$$

$$\partial_t \tilde{\gamma}_{ij} = -2\alpha \tilde{A}_{ij} + \tilde{\gamma}_{ik}\partial_j \beta^k + \tilde{\gamma}_{jk}\partial_i \beta^k - \frac{2}{3}\tilde{\gamma}_{ij}\partial_k \beta^k + \beta^k \partial_k \tilde{\gamma}_{ij}$$

$$\partial_t K = -D_i D^i \alpha + \alpha\!\left(\tilde{A}_{ij}\tilde{A}^{ij} + \frac{K^2}{3}\right) - \kappa_1(1+\kappa_2)\alpha\Theta + 4\pi\alpha(\rho + S) + \beta^i \partial_i K$$

$$\partial_t \Theta = \frac{1}{2}\alpha\!\left(R - \tilde{A}_{ij}\tilde{A}^{ij} + \frac{2}{3}K^2\right) - \alpha\hat{\Gamma}^\mu{}_{\mu\nu}Z^\nu - \kappa_1(2+\kappa_2)\alpha\Theta + \beta^i\partial_i\Theta$$

**Constraint damping parameters (production defaults):** κ₁ = 0.02, κ₂ = 0, η = 2.0.

### 4.2 Gauge Conditions

**1+log slicing (lapse):**
$$\partial_t \alpha = \beta^i \partial_i \alpha - 2\alpha K$$

This gauge is singularity-avoiding: α → 0 near the puncture, preventing numerical evolution from reaching the physical singularity.

**Gamma-driver (shift) — moving punctures:**
$$\partial_t \beta^i = \frac{3}{4} B^i$$
$$\partial_t B^i = \partial_t \tilde{\Gamma}^i - \eta B^i$$

The moving-puncture method (Campanelli et al. 2006, Baker et al. 2006) allows the coordinate puncture to move across the grid while the gauge automatically adjusts to maintain a regular solution.

### 4.3 GRMHD: Valencia Formulation

The matter sector evolves the conserved variables vector **U** via:

$$\partial_t \mathbf{U} + \partial_i \mathbf{F}^i(\mathbf{U}) = \mathbf{S}(\mathbf{U}, g_{\mu\nu})$$

where the conserved variables are:

$$\mathbf{U} = \sqrt{\gamma}\left(D,\, S_j,\, \tau,\, B^i,\, D Y_e\right)^T$$

with:
- $D = \rho W$ (conserved baryon density, W = Lorentz factor)
- $S_j = \rho h W^2 v_j + \epsilon_{jkl}(v^k - \beta^k/\alpha)B^l \sqrt{\gamma}/\alpha$ (momentum density)
- $\tau = \rho h W^2 - P - D$ (energy density)
- $B^i$ (magnetic field, staggered for CT)
- $Y_e$ (electron fraction, advected passively)

The geometric source terms **S** couple the matter to the spacetime curvature via Christoffel symbols and metric derivatives evaluated from the CCZ4 output.

### 4.4 Magnetic Field: Constrained Transport

The magnetic field evolves via:

$$\partial_t B^i + \partial_j\!\left(v^j B^i - v^i B^j\right) = 0$$

subject to the no-monopole constraint:

$$\partial_i B^i = 0$$

GRANITE enforces ∇·B = 0 to **machine precision** at all times using Constrained Transport (Evans & Hawley 1988). The magnetic field is stored on cell faces (staggered grid), and the CT update guarantees that the discrete divergence is exactly zero at every timestep, not approximately.

### 4.5 Radiation: M1 Moment Closure

The radiation field is described by its energy density E (measured in the fluid frame) and flux F^i, satisfying:

$$\partial_t E + \partial_i F^i = -\kappa_a (E - E_\mathrm{eq}) + Q_\mathrm{leakage}$$

$$\partial_t F^i + \partial_j P^{ij} = -(\kappa_a + \kappa_s) F^i$$

where P^{ij} is the Eddington tensor, closed analytically via the M1 prescription (Minerbo 1978, Levermore 1984):

$$P^{ij} = \frac{1-\xi}{2}\delta^{ij} E + \frac{3\xi - 1}{2} n^i n^j E$$

with the Eddington factor ξ = ξ(f) depending on the radiation flux factor f = |F|/(cE).

---

## 5. Numerical Approach

### 5.1 Spatial Discretization

All spatial derivatives are approximated using **4th-order centered finite differences**:

$$\left(\partial_x f\right)_i = \frac{-f_{i+2} + 8f_{i+1} - 8f_{i-1} + f_{i-2}}{12\Delta x} + \mathcal{O}(\Delta x^4)$$

This requires 4 ghost zone layers per face (`nghost = 4`).

### 5.2 Kreiss-Oliger Dissipation

High-frequency numerical noise is suppressed by adding Kreiss-Oliger dissipation to all CCZ4 right-hand sides:

$$\left(\partial_t u\right)_\mathrm{KO} = -\sigma \frac{(\Delta x)^{2p-1}}{2^{2p}} \sum_{\pm} D^{2p}_\pm u, \quad p = 3$$

**Critical:** σ = 0.1 is the safe default. Values σ > 0.1 over-dissipate the trumpet gauge profile — a confirmed failure mode documented in the CHANGELOG.

### 5.3 Time Integration: SSP-RK3

The 3rd-order Strong-Stability-Preserving Runge-Kutta scheme (Shu & Osher 1988):

$$U^{(1)} = U^n + \Delta t\, \mathcal{L}(U^n)$$
$$U^{(2)} = \frac{3}{4}U^n + \frac{1}{4}\left[U^{(1)} + \Delta t\, \mathcal{L}(U^{(1)})\right]$$
$$U^{n+1} = \frac{1}{3}U^n + \frac{2}{3}\left[U^{(2)} + \Delta t\, \mathcal{L}(U^{(2)})\right]$$

SSP-RK3 is total-variation-diminishing (TVD) and is the industry standard for HRSC methods in numerical relativity.

### 5.4 GRMHD Reconstruction

Interface values for the Riemann solver are reconstructed using:

| Method | Order | Use case |
|---|---|---|
| PLM (Piecewise Linear Method) | 2nd | Quick tests |
| PPM (Piecewise Parabolic Method) | 3rd | Standard production |
| MP5 (Monotonicity-Preserving 5th order) | 5th | High-resolution runs |

MP5 (Suresh & Huynh 1997) is the production default, providing near-spectral accuracy without spurious oscillations near discontinuities.

---

## 6. Multi-Messenger Observables

### 6.1 Gravitational Wave Extraction

GRANITE extracts gravitational waves via the Newman-Penrose Weyl scalar Ψ₄, decomposed in spin-weighted spherical harmonics:

$$\Psi_4(t,r) = \sum_{\ell,m} \Psi_4^{\ell m}(t,r) \,{}_{-2}Y_{\ell m}(\theta, \phi)$$

The dominant mode for non-precessing equal-mass BBH is the (ℓ,m) = (2,2) mode. Extraction is performed at multiple radii r_ex ∈ {50, 100, 150, 200, 300, 500} r_g to enable extrapolation to null infinity (Nakano et al. 2015).

The GW strain is recovered by double time-integration:

$$h_+ - ih_\times = \int_0^t \int_0^{t'} \Psi_4 \, dt'' \, dt'$$

using the fixed-frequency integration method (Reisswig & Pollney 2011) to suppress integration constants.

### 6.2 Apparent Horizon

The apparent horizon — the outermost surface where outgoing null rays have zero expansion — is located by solving:

$$\Theta \equiv D_i n^i + K_{ij}n^i n^j - K = 0$$

on a trial surface, iterated to convergence. The horizon mass and spin are computed from:

$$M_\mathrm{irr} = \sqrt{A_\mathrm{AH} / (16\pi)}, \qquad a = J / M^2$$

### 6.3 Constraint Norms

The Hamiltonian and momentum constraints:

$$\mathcal{H} \equiv R + K^2 - K_{ij}K^{ij} - 16\pi\rho = 0$$
$$\mathcal{M}^i \equiv D_j\!\left(K^{ij} - \gamma^{ij}K\right) - 8\pi j^i = 0$$

are monitored as L2 norms over the computational domain. These are the primary health diagnostics for every GRANITE simulation.

---

## 7. Physical Scales & Unit System

GRANITE uses **geometrized units** throughout: G = c = 1, with the total ADM mass M_total = 1.

| Quantity | Symbol | CGS Value for M = 10⁸ M☉ |
|---|---|---|
| Mass unit | M | 10⁸ M☉ = 2 × 10³⁸ g |
| Length unit | GM/c² | ~1.5 × 10¹³ cm = 1 AU |
| Time unit | GM/c³ | ~500 s |
| GW frequency unit | c³/(GM) | ~2 × 10⁻³ Hz |

For stellar-mass BH mergers (M ~ 30 M☉): length unit ~ 45 km, time unit ~ 150 μs, GW frequency ~ 6 kHz.

---

## 8. References

| Reference | Used for |
|---|---|
| Alic et al. 2012, PRD 85, 064040 | CCZ4 formulation |
| Baker et al. 2006, PRL 96, 111102 | Moving punctures |
| Berger & Oliger 1984, J. Comput. Phys. 53, 484 | AMR + subcycling |
| Bona et al. 1995, PRL 75, 600 | 1+log slicing gauge |
| Brandt & Brügmann 1997, PRL 78, 3606 | Two-Punctures initial data |
| Campanelli et al. 2006, PRL 96, 111101 | Moving punctures / Gamma-driver |
| Evans & Hawley 1988, ApJ 332, 659 | Constrained transport |
| Levermore 1984, J. Quant. Spectrosc. Radiat. Transfer 31, 149 | M1 closure |
| Minerbo 1978, J. Quant. Spectrosc. Radiat. Transfer 20, 541 | Eddington factor |
| Miyoshi & Kusano 2005, J. Comput. Phys. 208, 315 | HLLD Riemann solver |
| Nakano et al. 2015, PRD 91, 104022 | Ψ₄ extrapolation to null infinity |
| Reisswig & Pollney 2011, CQG 28, 195015 | Fixed-frequency GW integration |
| Shu & Osher 1988, J. Comput. Phys. 77, 439 | SSP-RK3 time integration |
| Suresh & Huynh 1997, J. Comput. Phys. 136, 83 | MP5 reconstruction |

---

*GRANITE v0.6.7 — Science & Physics Reference — April 27, 2026*
