# GRANITE — HPC Scaling Test Templates

This directory contains job scheduler templates and telemetry documentation for running GRANITE on distributed-memory supercomputers.

## Contents

| File | Purpose |
|------|---------|
| `slurm_submit.sh` | SLURM submission template (NERSC Perlmutter, ARCHER2, Jülich JURECA-DC) |
| `pbs_submit.sh`   | PBS/Torque submission template (legacy HPC centres) |
| `README.md`       | This document |

## Quick Start

1. **Build GRANITE** on the target system (see [INSTALLATION.md](../../INSTALLATION.md) or [docs/INSTALL.md](../../docs/INSTALL.md)).

2. **Choose a template** and edit the `USER CONFIGURATION` section:
   - Set `GRANITE_ROOT` to the absolute path of your checkout.
   - Set your HPC account/project code.
   - Adjust node count, tasks-per-node, and cpus-per-task for your scaling experiment.

3. **Submit**:
   ```bash
   sbatch slurm_submit.sh   # SLURM clusters
   qsub   pbs_submit.sh     # PBS/Torque clusters
   ```

## Manual NUMA / MPI Overrides

Both templates invoke `scripts/run_granite_hpc.py`, the dedicated HPC launcher that provides full administrator control over hardware topology.

| Flag | Effect |
|------|--------|
| `--omp-threads N` | Forces `OMP_NUM_THREADS=N`, overriding inherited shell value |
| `--mpi-ranks N` | Total MPI ranks — wraps binary with the chosen MPI launcher |
| `--disable-numa-bind` | Clears all NUMA/CPU affinity hooks; `numactl` is NOT invoked |
| `--numactl-args ARGS` | Custom `numactl` args (default: `--interleave=all`) |
| `--mpi-launcher CMD` | MPI executable: `mpirun` (default), `srun`, `aprun`, `mpiexec` |
| `--mpi-extra-args ARGS` | Extra flags forwarded verbatim to the MPI launcher |
| `--amr-telemetry-file PATH` | Output path for the AMR subcycling JSONL telemetry file |

## AMR Subcycling Telemetry

When `--amr-telemetry-file` is provided, `run_granite_hpc.py` writes one JSON object per line (JSONL format) capturing AMR parallel communication overhead in real-time:

```json
{"wall_s": 47.312, "mpi_rank": 0, "amr_level": 2, "subcycle_ms": 14.7, "blocks_synced": 256, "raw_line": "[AMR] subcycle level=2 sync=256 14.7ms"}
```

### Telemetry field reference

| Field | Type | Description |
|-------|------|-------------|
| `wall_s` | float | Seconds elapsed since job start (monotonic clock) |
| `mpi_rank` | int | MPI rank that emitted the event |
| `amr_level` | int | AMR refinement level that triggered the subcycle |
| `subcycle_ms` | float | Duration of the AMR synchronisation (milliseconds) |
| `blocks_synced` | int | Number of AMR blocks exchanged across MPI ranks |
| `raw_line` | str | Verbatim engine output line for debugging correlation |

### Post-processing (Python/Pandas)

```python
import json, pathlib
import pandas as pd

records = [
    json.loads(line)
    for line in pathlib.Path("amr_telemetry.jsonl").read_text().splitlines()
    if line.startswith("{")
]
df = pd.DataFrame(records)

# Per-level overhead summary
print(df.groupby("amr_level")["subcycle_ms"].describe())

# Communication fraction over time
df["comm_fraction"] = df["subcycle_ms"] / (df["wall_s"].diff() * 1000 + 1e-9)
df.plot(x="wall_s", y="comm_fraction", title="AMR Communication Overhead Over Time")
```

## Strong vs. Weak Scaling

| Mode | What to fix | What to vary | Target metric |
|------|-------------|--------------|---------------|
| **Strong** | Domain / params.yaml unchanged | Node count | Parallel efficiency (ideal: 1.0) |
| **Weak** | Work per rank (scale domain with nodes) | Node count + domain extent | Weak scaling efficiency (ideal: 1.0) |

**Strong scaling reference run** (Equal-mass BBH):
```bash
for NODES in 1 2 4 8 16; do
  sbatch --nodes=$NODES slurm_submit.sh
done
```

**Weak scaling**: create variants of `benchmarks/B2_eq/params.yaml` with grid resolutions that scale as `N^(1/3)` where `N` is the rank count.

## Tested Platforms

| System | Scheduler | Compiler | MPI | Status |
|--------|-----------|----------|-----|--------|
| NERSC Perlmutter | SLURM | GCC 11.2 | Cray MPICH 8.1 | Template verified |
| ARCHER2 | SLURM | GCC 11.2 | Cray MPICH 8.1.23 | Template verified |
| Jülich JURECA-DC | SLURM | GCC 11 | OpenMPI 4.1 | Template verified |
| Generic PBS cluster | PBS/Torque | GCC 10+ | OpenMPI 4+ | Template structure verified |

> To add a new platform or report a submission issue, open a GitHub issue tagged `[hpc-platform]` with your scheduler version, compiler, and MPI implementation.
