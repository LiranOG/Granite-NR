# GRANITE-NR — Validation Status
**Version:** v0.6.8 · **Date:** May 2026

"Wired into main loop" = the module is actually called during a production run.
"Dead-code" = compiled and linked but never called at runtime.

| Module | Implemented | Unit-tested | Wired into main loop | End-to-end validated | Target |
|--------|:-----------:|:-----------:|:--------------------:|:--------------------:|--------|
| CCZ4 spacetime evolution | ✅ | ✅ | ✅ | Partial (SP + BBH early-inspiral) | Active |
| GRMHD Valencia solver | ✅ | ✅ | ✅ | Partial (no disk validation) | Active |
| M1 radiation transport | ✅ | ✅ | ❌ | ❌ | v0.7 |
| Neutrino transport | ✅ | ✅ | ❌ | ❌ | v0.7 |
| Berger-Oliger AMR (subcycling) | ✅ | ✅ | ✅ | Partial | Active |
| Dynamic AMR regridding | Partial | ❌ | ❌ | ❌ | v0.7 blocker |
| Checkpoint write | ✅ | Partial | ✅ | Partial | Active |
| Checkpoint resume (--resume) | Partial | ❌ | ❌ | ❌ | v0.7 blocker |
| GW extraction (Ψ₄) | ✅ | Partial | ✅ | Partial | Active |
| Recoil velocity | ❌ (throws) | ❌ | ❌ | ❌ | v0.7+ |
| Horizon finder | ✅ | ✅ | ❌ dead-code | ❌ | v0.7 |
| Postprocess module | ✅ | ✅ | ❌ dead-code | ❌ | v0.7 |
| B2_eq benchmark | ✅ | n/a | ✅ | Early-inspiral only | Active |
| SXS:BBH:3634 validation | ❌ | ❌ | ❌ | ❌ | v0.8–v0.9 |
| B5_star scenario | Concept spec | ❌ | ❌ | ❌ | v1.0 flagship |

## Diagnostic Limitations
- **alpha_center** reads from AMR level 0, not the finest level near the puncture.
  Values ~0.94 in BBH runs are a diagnostic artifact, not a physics result.
  The true trumpet lapse (~0.3) requires finest-level sampling. Fix: v0.7.
- **ko_sigma: 0.35** in B2_eq is under active review. Parameter Reference
  recommends ≤ 0.1 for single-puncture stability.
