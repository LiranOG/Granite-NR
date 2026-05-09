# Reproducibility Artifact — single_puncture 64³ (v0.6.8)
Status: Pending — output files to be added after benchmark runs complete.

## Reproduction command
python3 scripts/run_granite.py run benchmarks/single_puncture/params.yaml

## Expected outputs (to be populated)
- params.yaml — exact input file used
- run.log — raw simulation log
- telemetry.csv — step-by-step diagnostics
- metrics.json — summary statistics with tolerances

## Expected metric tolerances (NRCF reference)
- Hamiltonian constraint ‖H‖₂ at t=100M: < 1e-3
- alpha_center at t=100M (finest AMR level): ~0.3 (trumpet value)
