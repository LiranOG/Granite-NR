# GRANITE Diagnostic Handbook

**Version:** v0.6.7 (current development) | v0.6.5 (last stable release)

> This handbook explains how to interpret GRANITE's real-time telemetry output,
> diagnose common failure modes, and distinguish physics problems from numerical
> artefacts. It is the primary reference for `granite_analysis.cli.sim_tracker` output and the
> pre-flight `health_check.py` report.

---

## Table of Contents

1. [Primary Health Indicators](#1-primary-health-indicators)
2. [The Lapse Lifecycle](#2-the-lapse-lifecycle-α)
3. [Hamiltonian Constraint Interpretation](#3-hamiltonian-constraint-interpretation)
4. [NaN Forensics](#4-nan-forensics)
5. [AMR and Grid Health](#5-amr-and-grid-health)
6. [Pre-flight Health Check Checklist](#6-pre-flight-health-check-checklist)
7. [Diagnostic Flowchart](#7-diagnostic-flowchart)
8. [Common Failure Modes and Resolutions](#8-common-failure-modes-and-resolutions)

---

## 1. Primary Health Indicators

The `granite_analysis.cli.sim_tracker` dashboard reports the following diagnostic quantities at
every output step. Each field has a defined healthy range and labelled status.

Invoke via:
```bash
python3 -m granite_analysis.cli.sim_tracker [logfile]  # from a log file
python3 scripts/run_granite.py dev                      # live integrated pipeline
```

| Field | Physical meaning | Status labels |
|---|---|---|
| `step` | Integration step count | — |
| `t` | Coordinate time in code units [M] | — |
| `α_center` | Lapse function at the finest AMR level near the puncture | `[HEALTHY]` `[COLLAPSE]` `[RECOVER]` |
| `‖H‖₂` | L2-norm of the Hamiltonian constraint | `[OK]` `[WARN]` `[HIGH]` `[CRIT]` |
| `‖M‖₂` | L2-norm of the momentum constraint | `[OK]` `[WARN]` `[HIGH]` |
| `Blocks` | Active AMR block count | — |
| `Phase` | Inspiral phase classification | `Early` `Mid` `Late Inspiral` |
| `speed` | Throughput in simulated M per wall-clock second | — |
| `ETA` | Estimated remaining wall time | — |
| `NaN events` | Count of non-finite values produced this step | Must be 0 |

### Throughput as a diagnostic

A sustained drop in throughput (M/s) during a run indicates increasing AMR
refinement — the engine is resolving stronger gradients, which increases the
Berger-Oliger subcycling overhead. This is expected behaviour during inspiral
and merger phases. A throughput drop accompanied by rising `‖H‖₂` is a
warning sign: the grid may not be refining aggressively enough to keep pace
with the evolving physics.

---

## 2. The Lapse Lifecycle (α)

In moving-puncture simulations with the 1+log gauge condition, the central
lapse `α_center` follows a characteristic three-phase trajectory. Deviations
from this trajectory are the earliest indicator of gauge pathology.

### Phase 1 — Initial state (t = 0)

```
α_center ≈ 1.0
```

Flat spacetime; proper time and coordinate time flow at the same rate.

### Phase 2 — Gauge collapse (0 < t ≲ 10 M)

```
α_center → 0.003  [COLLAPSE]
```

The 1+log slicing condition drives the lapse toward zero near the puncture,
effectively freezing the coordinate evolution near the singularity. This is
the correct and expected behaviour. The alert label `[COLLAPSE]` is
informational, not an error.

> [!IMPORTANT]
> A lapse collapse to α ≈ 0.003 is **not a failure**. It is the mechanism
> by which moving-puncture codes avoid evolving into the singularity.

### Phase 3 — Trumpet formation and recovery (t ≳ 15 M)

```
α_center → 0.28–0.35 (single BH) or 0.94–0.97 (BBH, level-0 reading)  [RECOVER]
```

The lapse recovers to a stationary value as the coordinate system settles into
the trumpet geometry. For a single Schwarzschild puncture at sufficient
resolution, the expected trumpet value is α_∞ ≈ 0.3. In BBH runs where
`α_center` is sampled from AMR level 0 (a known limitation in v0.6.7), the
reported value is larger (~0.94–0.97) because level 0 does not resolve the
puncture region. This is a diagnostic limitation, not a physics error.

### Pathological lapse behaviour

| Observation | Likely cause | Action |
|---|---|---|
| α_center stays at 1.0 indefinitely | Initial data issue — gauge not evolving | Check `1log_slicing` is active in params |
| α_center collapses to 0 and stays | Secondary gauge collapse | Check `ko_sigma ≤ 0.1`; check CFL |
| α_center exceeds 1.0 | Severe gauge instability | Halt run; check BCs and initial data compatibility |
| α_center oscillating ± 0.3 | Gauge wave reflection from boundary | Increase domain size; check Sommerfeld BCs |

---

## 3. Hamiltonian Constraint Interpretation

The Hamiltonian constraint

```
H = R + K² - K_ij K^ij - 16π ρ = 0
```

is the Einstein field equation projected onto constant-time hypersurfaces.
In an exact solution, H = 0 everywhere. Numerical discretisation introduces
nonzero residuals; the L2-norm ‖H‖₂ measures the magnitude of these residuals
averaged over the domain.

### Status thresholds

| `‖H‖₂` value | Status | Interpretation |
|---|:---:|---|
| < 0.1 | `[OK]` | Constraint well-satisfied at current resolution |
| 0.1 – 0.5 | `[WARN]` | Elevated; monitor growth rate |
| 0.5 – 2.0 | `[HIGH]` | Significant violation; check CFL, ko_sigma, BCs |
| > 2.0 | `[CRIT]` | Severe violation; constraint evolution has failed |
| NaN / ∞ | — | Numerical divergence; see Section 4 |

### Healthy evolution signatures

A well-behaved CCZ4 run shows one of the following ‖H‖₂ trajectories:

1. **Monotonic decay** — ‖H‖₂ decreases from its initial value throughout the
   run. This indicates active CCZ4 constraint damping is working correctly.
   The single-puncture 64³ benchmark achieves an 84.8× reduction over 500 M.

2. **Initial transient followed by decay** — ‖H‖₂ rises during the gauge
   adjustment phase (t ≲ 20 M) then decays. This is the expected behaviour in
   BBH runs. The peak at t ≈ 6 M in B2_eq is normal.

3. **Stable plateau** — ‖H‖₂ remains approximately constant at a low value.
   Acceptable for long-duration runs if the plateau level is below 0.1.

### Constraint growth rate γ

The exponential growth rate of ‖H‖₂ is fit as:

```
‖H‖₂(t) ~ exp(γ t)
```

Reported in the diagnostic output as `γ [/M]`. Interpretation:

| γ [/M] | Assessment |
|---|---|
| < 0 | Constraint decay — optimal |
| 0 to 0.005 | Stable — acceptable for production runs |
| 0.005 to 0.02 | Slow growth — monitor; may be transient |
| > 0.02 sustained | Instability — investigate cause |

---

## 4. NaN Forensics

A NaN (Not-a-Number) or Inf value in any evolved variable indicates a
numerical divergence. GRANITE's NaN forensics system localises the first
occurrence and reports the affected variable and grid location.

### NaN propagation model

NaN values propagate through finite-difference stencils: a NaN at grid point
(i,j,k) at step n will contaminate all points within a 2-cell radius at step
n+1 (for 4th-order stencils). Detection at the step boundary is therefore
essential. GRANITE performs a finite-check pass after every RK3 substep.

### Forensic output fields

```
[NaN DETECTED] step=42  t=6.56M
  Variable: LAPSE (index 18)  Location: (32, 32, 32)
  Spacetime RHS: all finite at t=0  ✓
  Hydro RHS: all finite at t=0      ✓
  First infected variable: CHI (index 0) at step 40
```

### Diagnostic procedure

**Step 1 — Check RHS at t=0.**
If `[DIAG] Spacetime RHS: all finite` and `[DIAG] Hydro RHS: all finite` are
both confirmed in the initialisation log, the initial data is valid. A NaN
originating at t=0 indicates an initial data construction error.

**Step 2 — Identify the first infected variable.**
`CHI` (conformal factor, index 0) failure near the puncture → resolution
insufficient; increase AMR levels or reduce `ko_sigma`.
`LAPSE` (index 18) failure → CFL violation or secondary gauge collapse.
`BX/BY/BZ` (magnetic, indices 5–7) failure → GRMHD reconstruction failure;
switch from MP5 to PPM.

**Step 3 — Localise the grid point.**
- Failure at the domain boundary → check Sommerfeld boundary condition
  compatibility with the initial data type.
- Failure at AMR level boundary → check prolongation and ghost zone filling.
- Failure at the puncture location → check AMR tracking sphere coverage and
  finest-level resolution.

**Step 4 — Check the CFL condition.**
```
CFL_finest = dt_base / (2^(n_levels-1) * dx_finest) ≤ 0.5
```
A CFL violation at the finest level is the most common NaN cause in BBH runs.

> [!NOTE]
> The final simulation summary may report "No NaN events detected" even when
> NaN values appear in the step log. This can occur if the run terminates before
> the tracker flushes the event buffer. **Always verify by inspecting the
> step-by-step log directly.** The step log takes precedence over the summary.

---

## 5. AMR and Grid Health

### Block count evolution

The AMR block count should increase from its initial value (typically 3–4) as
refinement triggers are satisfied near the punctures. A block count that does
not increase beyond the initial value throughout a BBH run indicates that the
refinement criterion threshold is too conservative or that dynamic regridding
is not active.

> [!NOTE]
> Dynamic regridding is the default in v0.6.7. If block count is frozen,
> check that `regrid_interval` is set in params.yaml and is less than the
> total step count.

### Tracking sphere coverage

GRANITE registers a tracking sphere of radius R_track = max(2 M_BH, 0.5 M_ADM)
around each puncture at initialisation. The AMR hierarchy guarantees at least
`min_level` refinement within this sphere throughout the run.

If the run log shows a tracking sphere registered at initialization but the
finest AMR level does not cover the puncture at later times, the tracking
sphere is not moving with the puncture — this is a known limitation of the
current AMR implementation and is tracked as a v0.7 development item.

### CFL guard events

A CFL guard event occurs when the adaptive timestep controller detects that
the wave speed at the finest level exceeds the CFL stability bound. GRANITE
logs these as:

```
[CFL GUARD] step=38  t=5.94M  ratio=1.73  (dt reduced)
```

A ratio greater than 1.0 indicates the timestep was reduced. A ratio greater
than 10.0 indicates a severe CFL violation. Any ratio exceeding 100 signals
that the numerical evolution has become unphysical and the run should be halted.

---

## 6. Pre-flight Health Check Checklist

Run `python3 scripts/health_check.py` before every simulation. The output
must satisfy all of the following before the run is started.

| Check | Expected output | Failure action |
|---|---|---|
| Build type | `Release` | Rebuild with `--release` flag |
| Optimisation flags | `-O3 -march=native -DNDEBUG` | Rerun `run_granite.py build` |
| OpenMP thread count | Equal to physical core count | Set `OMP_NUM_THREADS=$(nproc)` |
| OpenMP binding | `OMP_PROC_BIND=true` | Add to shell environment |
| HDF5 availability | `libhdf5 found (parallel)` | Reinstall `libhdf5-openmpi-dev` |
| MPI availability | `mpirun found` | Install OpenMPI |
| Filesystem location | Linux-native path | Move out of `/mnt/c/` (WSL2) |
| Memory headroom | ≥ 2 GB free | Close other processes |

If any check fails, do not start the simulation. Simulations run in Debug mode
or from Windows-mounted paths may complete but will produce incorrect results
or fail silently.

---

## 7. Diagnostic Flowchart

```
Run started — monitoring telemetry
│
├─ NaN events > 0?
│   ├─ YES: → Check CFL_finest ≤ 0.5
│   │        → Check ko_sigma ≤ 0.1
│   │        → Switch reconstruction: MP5 → PPM → PLM
│   │        → See Section 4 for full procedure
│   └─ NO:  → Continue
│
├─ ‖H‖₂ growing (γ > 0.02/M sustained)?
│   ├─ YES: → Is domain half-width ≥ 48 M?
│   │        → Is ko_sigma ≤ 0.1?
│   │        → Are BCs compatible with initial data? (Sommerfeld + BY, Copy + BL)
│   │        → See Section 8 for failure mode guide
│   └─ NO:  → Continue
│
├─ α_center not recovering from collapse?
│   ├─ YES: → Is ko_sigma ≤ 0.1? (0.35 destroys trumpet profile)
│   │        → Is resolution sufficient? (dx_finest ≲ 0.2 M for trumpet)
│   │        → Check for secondary gauge collapse (α → 0 permanently)
│   └─ NO:  → Continue
│
├─ No BBH inspiral (BHs stationary)?
│   ├─ YES: → Check tangential BY momenta p_t ≈ ±0.0954 per BH (d=10M)
│   │        → Zero momenta → head-on collision, not inspiral
│   └─ NO:  → Continue
│
└─ All checks pass → Run is healthy
```

---

## 8. Common Failure Modes and Resolutions

### F1 — Crash at t ≈ 5–7 M

**Symptoms:** NaN events appear; `‖H‖₂` jumps by orders of magnitude; run
terminates with `[CRASH]` or hangs.

**Cause:** CFL violation during the lapse collapse phase. The lapse drops
toward zero, making the numerical wave speed effectively infinite, violating
the Courant condition.

**Resolution:** Reduce the global CFL factor to 0.15. Verify that the finest
AMR level CFL satisfies `dt_finest / dx_finest ≤ 0.5`. Alternatively, reduce
the number of AMR levels by one to increase `dt_finest`.

---

### F2 — Constraint explosion from t = 0

**Symptoms:** `‖H‖₂` begins growing exponentially from the first output step;
no NaN events initially but run terminates within 50 M.

**Cause (most common):** Sommerfeld boundary conditions applied with
Brill-Lindquist initial data. These are incompatible: BL data has a different
asymptotic conformal structure than Sommerfeld BCs assume.

**Resolution:** Set `boundary: type: copy` for all runs using Brill-Lindquist
initial data. Sommerfeld BCs are only compatible with Two-Punctures /
Bowen-York initial data.

---

### F3 — Over-dissipation destroying the trumpet gauge

**Symptoms:** `α_center` collapses and never recovers. `‖H‖₂` initially
decays but then grows again as the gauge becomes unphysical. No NaN events.

**Cause:** `ko_sigma` set above the safe maximum (default 0.10). Values of
0.35 or higher dissipate the trumpet gauge profile along with the high-frequency
noise they are intended to remove.

**Resolution:** Reset `ko_sigma: 0.10` in params.yaml. This is the
production default and must not be increased without explicit testing. This
failure mode was confirmed and permanently documented in the CHANGELOG.

---

### F4 — No BBH inspiral

**Symptoms:** The two punctures remain approximately stationary or undergo
a head-on collision rather than an orbit.

**Cause:** Zero tangential Bowen-York momenta. The `momentum` fields in
params.yaml default to `[0.0, 0.0, 0.0]`, which produces a head-on
collision or quasi-static configuration.

**Resolution:** Set quasi-circular tangential momenta. For an equal-mass
binary at d = 10 M:
```yaml
bh1:
  momentum: [0.0, 0.0954, 0.0]
bh2:
  momentum: [0.0, -0.0954, 0.0]
```
The value p_t ≈ 0.0954 M is the leading-order post-Newtonian approximation
for d = 10 M equal-mass separation.

---

### F5 — Boundary constraint growth in long runs

**Symptoms:** `‖H‖₂` is stable for t < 100 M but begins growing after a
gauge wave reflected from the outer boundary reaches the interior.

**Cause:** Domain too small. Gauge waves travel at characteristic speed
v_gauge = √2 in CCZ4. The reflection travel time is t_reflect = L / √2,
where L is the domain half-width.

**Resolution:** For runs to t_final = 500 M, the domain half-width must
satisfy L ≳ 500 / √2 ≈ 354 M. The B2_eq benchmark uses L = 200 M with
Sommerfeld BCs, which provides adequate attenuation. The single-puncture
benchmark uses L = 48 M, which is sufficient for t_final ≤ 500 M at 64³.

---

### F6 — α_center stuck at 0.94 (not forming trumpet)

**Symptoms:** `α_center` reported near 0.94 throughout the run; the expected
trumpet value of ~0.3 is never seen.

**Cause:** This is a resolution and diagnostic artefact, not a physics
failure. At 128³ with dx_finest = 0.094 M, the trumpet geometry near the
puncture (scale ~0.05 M) is marginally resolved. Additionally, in v0.6.7,
`α_center` is sampled from AMR level 0, not the finest level near the
puncture. The level-0 lapse does not reach the trumpet asymptote.

**Assessment:** Not a bug. The constraint norms and GW extraction are
unaffected. See the v0.7 roadmap for the `alpha_center` diagnostic fix.

---

*GRANITE v0.6.7 — Diagnostic Handbook*
*For simulation health interpretation, NaN forensics, and failure mode diagnosis.*
