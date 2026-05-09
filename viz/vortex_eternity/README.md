<div align="center">

# VORTEX 🌀
### **Zero-Allocation Post-Newtonian N-Body Simulation Engine**
#### *The Interactive WebGL Frontend for the GRANITE Numerical Relativity Ecosystem*
#### **File:** `VORTEX-engine.html` — open directly in any modern browser, no install required

[![License](https://img.shields.io/badge/License-GPL--3.0-21e6c1.svg)]()
[![Status](https://img.shields.io/badge/Status-Active%20Development-blue.svg)]()

[![Engine](https://img.shields.io/badge/Engine-Custom_WebGL-1f4287.svg)]()
[![Integrator](https://img.shields.io/badge/Integrator-Hermite--4_PC-1f4287.svg)]()
[![Physics](https://img.shields.io/badge/Physics-2.5PN_Relativity-278ea5.svg)]()
[![Precision](https://img.shields.io/badge/Precision-Kahan_Summation-278ea5.svg)]()
[![Audio](https://img.shields.io/badge/Audio-GW_Chirp_Synth-334756.svg)]()

</div>

---

## 📖 Table of Contents

1. [Abstract](#-abstract)
2. [Core Architecture & Physics Engine](#️-core-architecture--physics-engine)
3. [Astrophysical Phenomena](#-astrophysical-phenomena)
4. [Live Telemetry Matrix](#-live-telemetry-matrix)
5. [Simulation Scenarios](#-simulation-scenarios)
6. [Cinematic & Recording Systems](#-cinematic--recording-systems)
7. [Scientific Analysis Tools](#-scientific-analysis-tools)
8. [Tactical Minimap 3.0](#️-tactical-minimap-30)
9. [Interaction & Controls](#-interaction--controls)
10. [Keyboard Reference](#️-keyboard-reference)
11. [Comparison with Related Tools](https://github.com/LiranOG/Granite/blob/main/viz/vortex_eternity/README.md#%EF%B8%8F-keyboard-reference)
12. [Known Limitations](https://github.com/LiranOG/Granite/blob/main/viz/vortex_eternity/README.md#%EF%B8%8F-known-limitations-v07)
13. [Roadmap to v1.0](#-roadmap-to-v10)

---

## 📖 Abstract

**VORTEX** is an elite, high-performance N-Body simulation and visualization engine operating entirely within the browser — no installation, no server, no dependencies beyond a single HTML file. Designed as the interactive vanguard frontend for the **GRANITE** supercomputing ecosystem, VORTEX bridges the gap between intense computational astrophysics and frictionless, real-time 3D rendering.

Operating strictly on a **Zero-Allocation Architecture** — mutating pre-allocated mathematical vectors in-place across every hot-path — VORTEX completely bypasses JavaScript GC stalls, delivering a consistent 60 FPS integration of highly non-linear orbital dynamics, strong-field post-Newtonian relativity, and gravitational wave acoustics, all simultaneously.

VORTEX is not meant to be a toy. Every subsystem — from the Hermite integrator to the WebGL lens shader to the Web Audio chirp synthesizer — is engineered with the same rigor and architectural discipline applied to the GRANITE C++ backend.

> [!IMPORTANT]
> **Developer's Note:** VORTEX was born during an intense, focused sprint and is under active development. Every system documented here is functional and tested. Some edge cases may still exist. Bug reports, suggestions, and collaboration inquiries are welcome via the GRANITE repository Issues tracker.

---

## ⚙️ Core Architecture & Physics Engine

VORTEX is not a simple Newtonian gravity simulator. It is an actively dissipative, post-Newtonian relativistic engine designed to model the extreme dynamics of compact objects — Black Holes, Neutron Stars, SMBHs — in the strong-field regime.

### 1. The Integrator: Hermite 4th-Order Predictor-Corrector

Standard Runge-Kutta methods fail to maintain symplectic stability in highly chaotic N-Body environments. VORTEX employs the **Hermite 4th-Order Predictor-Corrector** scheme (Makino & Aarseth 1992), which computes both acceleration ($a$) and its time derivative, the jerk ($\dot{a}$).

- **Aarseth Adaptive Timestepping:** The integration step dynamically scales according to $\Delta t = \eta \sqrt{|a|/|\dot{a}|}$ to resolve ultra-close encounters without energy blow-up.
- **Multi-Level Bisection Recovery:** If a singularity causes numerical overflow, the engine reverts to the previous step and bisects the timestep up to 6 depth levels. If the singularity persists, the corrupted body is surgically excised to preserve global simulation health.

### 2. Numerical Resilience

- **Kahan Compensated Summation:** Global Hamiltonian energy accumulators use Kahan summation to eliminate $O(N \cdot \epsilon_{mach})$ catastrophic cancellation inherent in IEEE-754 floating-point arithmetic.
- **NaN-Shielding:** All forces, accelerations, and PN correction terms are guarded with finite-value checks before accumulation. NaN propagation is caught at the per-step level, never allowed to contaminate the state vector.

### 3. Post-Newtonian (PN) Relativity

VORTEX dynamically augments Newtonian gravity with analytically derived post-Newtonian correction terms:

| Order | Name | Effect |
|---|---|---|
| **1PN** | Einstein-Infeld-Hoffmann (EIH) | Perihelion precession, relativistic mass-energy equivalence. Conservative. $(v/c)^2$ corrections. Analytical jerk derived from EIH (Will 2014, LRRGR §9). |
| **1.5PN** | Lense-Thirring Spin-Orbit | Frame-dragging. Spin vectors actively perturb the orbital plane. Spin evolution via exact **Rodrigues' rotation formula** — no Euler-step magnitude inflation. (Barker-O'Connell 1975) |
| **2PN** | Conservative EIH + Spin-Spin + QM | Second-order conservative EIH corrections $(v/c)^4$. **Spin-spin coupling** (quadrupole-quadrupole). **Quadrupole moment** (`qm`) corrections. Toggled independently via `spinspin` and `qm` flags. |
| **2.5PN** | Radiation Reaction | Dissipative terms modelling gravitational wave emission. Drives binary inspiral and eventual merger. 2.5PN jerk computed via second-order finite-difference of stored prior-step acceleration. |

---

## 🔭 Astrophysical Phenomena

| Phenomenon | Physics Model |
|---|---|
| **Mass-Defect Mergers** | $\Delta M_{rad} \approx 0.048 (4\eta)^2 M$ radiated as GW energy on merger. Momentum conservation yields asymmetric recoil kick. |
| **GW Recoil Kicks** | Post-merger remnant receives velocity kick from asymmetric radiation. Can eject remnant from host system in extreme mass-ratio scenarios. |
| **Tidal Disruption Events (TDE)** | Roche limit: $d \le 2.44 R(M/m)^{1/3}$. Star crossing SMBH Roche limit undergoes spaghettification → Novikov-Thorne accretion disk. |
| **Quasi-Normal Modes (QNM)** | Post-merger ringdown frequency ($\omega$) and damping time ($\tau$) of the newly formed Kerr black hole. |
| **Gravitational Lensing** | Real-time GLSL ray-deflection shader: Einstein rings, photon sphere, Schwarzschild shadow. Activates on compact objects with compactness $C = GM/c^2R \ge 0.25$. |
| **Einstein Cross** | 4 secondary image samples at Einstein ring radius. Activates when `lensStr > 0.18` (strong-field threshold). |
| **Relativistic Doppler Shift** | Per-body LOS velocity component $\beta = \dot{r}\cdot\hat{n}/c$: receding → redder, approaching → bluer. GPU-side, zero-allocation. |
| **Relativistic Beaming** | Lorentz aberration brightness modulation: $I_{obs} \propto (1 + \beta\cos\theta)^4$. CPU-computed per-body per-frame. |

---

## 📊 Live Telemetry Matrix

The Mission Control dashboard provides real-time readout of every key observable:

### Global System

| Metric | Physical Definition |
|:---|:---|
| **Hamiltonian $H(q,p)$** | Total invariant energy, tracked via Kahan summation. |
| **Symplectic Error** $\|\Delta E/E_0\|$ | Absolute fractional Hamiltonian drift. $< 10^{-6}$ = excellent integrator health. |
| **Geometric Lapse** $\alpha_{min}$ | $\sqrt{1 - 2\Phi/c^2}$. Maximal time dilation. $\alpha \to 0$ = approach to event horizon. |
| **PN Parameter $x$** | $\max(v^2/c^2,\, GM/rc^2)$. $x > 0.1$ → PN approximation begins to break down → GRANITE backend required. |

### Dominant Binary & Gravitational Waves

| Metric | Physical Definition |
|:---|:---|
| **$L_{GW}$ (Peters Quadrupole)** | Total GW luminosity radiating from the dominant bound pair. |
| **$M_{rad}$ (Mass Defect)** | Cumulative mass converted to GW energy over the simulation lifetime. |
| **Strain $h_+,\, h_\times$** | Instantaneous quadrupole GW strain components. |
| **$f_{GW}$** | Dominant gravitational wave emission frequency. Feeds the audio chirp synthesizer. |
| **Chirp Mass $M_c$** | $(m_1 m_2)^{3/5}/(m_1+m_2)^{1/5}$. Used with Peters' timescale to predict $T_{merge}$. |
| **$\|S_{total}\|$ & $\chi_{eff}$** | Total spin angular momentum magnitude and effective dimensionless spin projected onto the orbital axis. |

---

## 🌌 Simulation Scenarios

VORTEX ships with a curated library of physically calibrated pre-set scenarios, covering the full spectrum of compact-object astrophysics:

| Scenario | Bodies | Physics Highlight |
|---|:---:|---|
| **GW150914** | 2 BH | LIGO's first detection. Equal-mass 36+29 M☉, $d=410$ Mpc. |
| **GW170817** | 2 NS | Binary neutron star merger. Kilonova + multi-messenger. |
| **Cygnus X-1** | BH + Star | Stellar-mass BHB with Roche-lobe mass transfer. |
| **Genesis** | 4–6 | Chaotic multi-body triple/quadruple with ejections. |
| **Stellar Nursery** | 8+ | Dense stellar cluster with ongoing dynamical friction. |
| **Galactic Core** | SMBH + swarm | SMBH + surrounding stellar population. TDE trigger. |
| **Solar System Macro-Cosmos** | 8+ | $5\times$ expanded frustum for solar-system-scale integrations. |
| **Sandbox (Solar Forge)** | Any | Full custom: place any body type with drag-to-launch velocity. |

---

## 🎬 Cinematic & Recording Systems

VORTEX v0.6.7 introduced a complete set of professional-grade capture and presentation tools.

### Zen Mode (`Z`)
Press `Z` at any time to instantly toggle Zen Mode. The **entire UI** — top bar, panels, telemetry, minimap, shortcuts bar — fades out in 0.5 seconds via a CSS opacity transition. The WebGL canvas, vignette, and atmospheric haze remain fully visible. A brief `✦ ZEN MODE` toast notification confirms activation and auto-fades after 3 seconds.

Zen Mode is designed specifically for:
- **Screen recording and streaming** — no HUD clutter
- **Scientific photography** — clean visualization for papers/talks
- **Demoing** — letting the simulation speak for itself

Press `Z` again to restore the full interface instantly.

### Cinematic Autopilot Camera (`🎬 Autopilot`)
A fully automated camera director. Activate via **Camera Modes → 🎬 Autopilot**. The camera automatically:
1. Selects interesting targets preferring Black Holes and Neutron Stars
2. Smoothly lerps toward each target (`lerp factor = 0.04` for cinematic pacing)
3. Slowly orbits the target (`autoRotateSpeed = 0.5`)
4. Switches targets every 12–15 seconds (jittered to avoid periodicity)
5. Logs each target switch to the Event Log

Combine with **Zen Mode** for fully autonomous, UI-free cinematic recordings.

### Gravitational Wave Audio Sonification (`🔊`)
The GW Chirp Synthesizer converts the physics of gravitational wave emission directly into sound:

- **Frequency:** $f_{audio} = \text{clamp}(f_{GW} \times 10,\; 30\text{ Hz},\; 1200\text{ Hz})$
  — deep 30 Hz inspiral rumble → 1200 Hz final chirp squeal
- **Amplitude:** $A = \text{clamp}(|h_+| \times 5\times 10^6,\; 0,\; 0.5)$
  — strain amplitude directly drives volume
- **Zero-Allocation:** All parameter updates via `AudioParam.setTargetAtTime()` — no GC pressure
- **Auto-mute:** Silences smoothly when no bound binary exists

**Volume Control Panel:** Click `🔊` in the top bar to open the volume popup. Adjust the slider (0–100%), mute/unmute independently, or silence completely. The top-bar icon updates dynamically: `🔇 🔈 🔉 🔊`.

---

## 📐 Scientific Analysis Tools

### NR Diagnostics Window (`🔭 Scientific Analysis → NR Diagnostics`)

Three live Chart.js panels updated every frame:

| Chart | Axes | Scientific Purpose |
|---|---|---|
| **Orbital Evolution** | Eccentricity $e$ vs. Semi-major axis $a$ | Traces the GW-driven inspiral track from initial orbit to merger. |
| **GW Chirp** | $f_{GW}$ vs. Simulation Time (log Y) | Captures the characteristic frequency sweep through merger. |
| **Relativistic Parameter** | $\beta^2 = (v/c)^2$ vs. Time | Confirms post-Newtonian regime entry and approach to merger. |

All three charts support time-filter decimation: `● LIVE / 1YR / 10YR / MAX`. Telemetry resets cleanly on every scenario switch.

### Mission Control
Full energy budget: Kinetic Energy, Potential Energy, Hamiltonian drift, angular momentum, linear momentum, GW strain columns — all live.

### Event Log
At-a-glance event stream: mergers, TDEs, ISCO warnings, Autopilot target switches, audio init, placement aborts.

---

## 🗺️ Tactical Minimap 3.0

The minimap is not a simple radar — it is a multi-spectral scientific sensor.

### Sensor Modes

| Mode | Formula | Purpose |
|---|---|---|
| **Bodies** (default) | Position projection | Standard body-dot radar |
| **〈〉 Isobar** | $\Phi = -\sum GM_i/r_i$ | 8-level gravitational potential contour lines (marching squares) |
| **🌡 Flux** | $F = \sum L_i / 4\pi r_i^2$ | Bolometric radiation flux with thermal colormap |

### Advanced Features

- **Camera Frustum Overlay** — Yellow translucent cone showing the main camera's FOV and orientation
- **ISCO Proximity Warnings** — Pulsing red reticle when any body enters $5\times$ ISCO ($6GM/c^2$)
- **Click-to-Select** — Click any body on the minimap to instantly follow it in the main camera
- **Hover Tooltip** — Shows `Name / Mass (M☉) / Orbital v (c) / Gravitational z / Type` on hover
- **Velocity Arrows** — Scaled directional arrows per body
- **⊚ LOG Toggle** — Logarithmic projection: $R_{pixel} = \log(1+R_{sim})$ — see tight binaries AND distant perturbers simultaneously

---

## 🎮 Interaction & Controls

### Camera & Flight Modes

| Mode | Activation | Behavior |
|---|---|---|
| **Free Orbit** | Default | Standard OrbitControls |
| **Track CoM** | Button / `数字1` | Smoothly tracks the system barycenter |
| **Binary Tracking** | Button | Locks focus on dominant binary CoM |
| **Follow Body** | Click body | Tracks selected body; minimap click also triggers this |
| **Cinematic** | Button | Slow smooth orbit, `autoRotate` |
| **🎬 Autopilot** | Button | Fully automated director — see above |
| **WASD Flight** | `E` | FPS drone mode: `WASD` move, Mouse look, `Shift` sprint, `Space` creep, `Esc` exit |

### The Sandbox & Solar Forge

Toggle **Sandbox Mode** (`N`) to pause the simulation and enter the Solar Forge:

1. **Select a Template:** Stars, Neutron Stars, Black Holes (SMBH), or Planets
2. **Tune Parameters:** Mass, Radius, Spin ($\chi$), and Luminosity
3. **Hologram Placement:** Click the geometric $Y=0$ plane to place
4. **Drag for Velocity:** Live **Keplerian Osculating Orbit** prediction (Ellipse / Parabola / Hyperbola) shown before you release
5. **ESC to Abort:** Press `Escape` at any point during placement to cancel cleanly

### Visual Modifiers

| Feature | Key | Description |
|---|:---:|---|
| Trails | `T` | Body trajectory trails |
| Grid | `G` | Reference spatial grid |
| Velocity Vectors | `V` | Velocity arrows on all bodies |
| Gravitational Lensing | `L` | Real-time GLSL spacetime curvature shader |
| Minimap | `M` | Tactical multi-spectral sensor |
| Bloom | `B` | HDR-style bloom post-processing |
| **Zen Mode** | `Z` | Full UI fade for distraction-free recording |

---

## ⌨️ Keyboard Reference

### Simulation

| Key | Action |
|---|---|
| `Space` | Pause / Resume |
| `R` | Reset to current scenario |
| `G` | Toggle grid |
| `T` | Toggle trails |
| `V` | Toggle velocity vectors |
| `L` | Toggle gravitational lensing |
| `M` | Toggle minimap |
| `B` | Toggle bloom |
| `S` | Toggle spawn mode |
| `N` | Toggle sandbox (Solar Forge) |
| `Z` | Toggle Zen Mode |

### Camera

| Key | Action |
|---|---|
| `1` | Free orbit |
| `2` | Track center of mass |
| `3` | Track dominant binary |
| `4` | Follow selected body |
| `5` | Cinematic orbit |
| `6` | 🎬 Autopilot |
| `E` | Enter / Exit WASD flight |

### Sim-OS & Interface

| Key | Action |
|---|---|
| `[` | Open / Close Sim-OS Master Menu (`≡`) |
| `?` | Open Keyboard Shortcuts reference |
| `Esc` | Close any open modal / cancel placement |

---

## ⚠️ Known Limitations (v0.7)

| Limitation | Impact | Planned Fix |
|---|---|---|
| $O(N^2)$ force computation | Frame rate degrades above ~30 bodies | Barnes-Hut / GPU BVH (v0.7) |
| PN approximation breaks down at $x > 0.1$ | Not suitable for plunge-phase waveforms | Full CCZ4 via GRANITE JSON playback (v1.0) |
| WebGL minimap uses Canvas 2D | Heatmap resolution scales with canvas size | WebGL fragment shader backend (v0.7) |
| Vertex shader Doppler uses CPU path | No GPU interpolation between frames | ShaderMaterial rewrite (v0.7) |
| MACRO-COSMOS mode + large body count | Heavy CPU/GPU load on low-end hardware | Instanced BVH + worker threads (v0.8) |
| Pointer lock drag requires HTTPS in some browsers | Drag may fall back to non-locked mode on `file://` | Documented user advisory |

---

## 🔮 Roadmap to v1.0

```
v0.6.7  ── Gold Master (current)
            Zen Mode, GW Audio Synth, Cinematic Autopilot,
            NR Diagnostics, Minimap 3.0, FWM Hardening

v0.7.0  ── Performance & Shader Upgrade
            Barnes-Hut O(N log N), GPU Doppler vertex shader,
            WebGL minimap fragment shader backend,
            Checkpoint / scenario export (JSON)

v0.8.0  ── GRANITE Integration Bridge
            Python granite_analysis → JSON kinematic stream
            WebSocket real-time HPC steering interface
            Apparent horizon shape import from HDF5

v1.0.0  ── Unified Visualization Dashboard
            Full GRANITE playback: CCZ4 trajectories,
            Ψ₄ GW strain rendering, AMR mesh overlay,
            Multi-messenger joint visualization (GW + EM + ν)
```

---

## 📎 Citing VORTEX

```bibtex
@software{vortex2026,
  author    = {LiranOG},
  title     = {{VORTEX}: Zero-Allocation Post-Newtonian N-Body
               Simulation Engine},
  year      = {2026},
  version   = {v0.6.7},
  url       = {https://github.com/LiranOG/Granite},
  note      = {WebGL frontend for the GRANITE numerical relativity
               ecosystem. 2.5PN dynamics, GW sonification, real-time
               lens shader, Cinematic Autopilot.}
}
```

---

<div align="center">

*"Simulate the unimaginable."*

**VORTEX v0.6.7 · 2026 · LiranOG**

[![GRANITE Backend](https://img.shields.io/badge/Backend-GRANITE_NR_Engine-0d6efd.svg)](https://github.com/LiranOG/Granite)
[![Wiki](https://img.shields.io/badge/Docs-GRANITE_Wiki-7c3aed.svg)](https://github.com/LiranOG/Granite/wiki)

</div>
