---
name: 🐛 Bug Report
about: Report a crash, wrong numerical result, NaN event, or unexpected behaviour in GRANITE
labels: bug
---

> [!IMPORTANT]
> **Before submitting:** Please search the [Issue Tracker](https://github.com/LiranOG/Granite/issues) and the `Known-Fixed-Bugs` wiki page to ensure this is not a duplicate or already resolved issue.

## 🚨 What Happened

<!-- Describe the bug clearly. Include what you expected vs. what you got. -->

## 🔬 Reproduction Steps

<!-- Provide the exact commands you ran. Include the `params.yaml` or benchmark name if applicable. -->

```bash
# Example:
git clone https://github.com/LiranOG/Granite && cd Granite
python3 scripts/run_granite.py build --release
python3 scripts/run_granite.py run --benchmark B2_eq
# or exact cmake + ctest commands
```

## 💻 Environment

| Field | Value |
|---|---|
| **GRANITE Version** | `v0.6.7` (or provide git sha) |
| **OS** | <!-- e.g. Ubuntu 22.04, WSL2 on Windows 11 --> |
| **Compiler** | <!-- e.g. GCC 12.3.0 (`gcc --version`) --> |
| **MPI** | <!-- e.g. OpenMPI 4.1.2 (`mpirun --version`) --> |
| **CMake Build Type** | <!-- Debug / Release / RelWithDebInfo --> |
| **OpenMP Threads** | <!-- e.g. OMP_NUM_THREADS=6 --> |

## 📜 Relevant Log Output

> [!NOTE]
> GRANITE logs are automatically saved to `dev_logs/sim_tracker_<timestamp>.log`.
> Please include the last ~50 lines if there is a NaN event or crash.

```text
(Paste log output here)
```

## 🧩 Additional Context

<!-- Any other context — e.g. the specific params.yaml values, hardware specs, or whether the issue is reproducible with a minimal grid (nx=8). -->
