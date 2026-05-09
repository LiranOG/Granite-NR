#!/usr/bin/env python3
# =============================================================================
#  run_granite_hpc.py — GRANITE HPC Launch Wrapper
# =============================================================================
#
#  Author  : Liran M. Schwartz
#  Version : 0.6.7  (tracks engine version)
#  License : GPL-3.0 (GRANITE engine) / MIT (this wrapper)
#
#  Purpose
#  -------
#  HPC launch wrapper for the GRANITE binary. Provides MPI integration,
#  optional NUMA binding, AMR subcycling telemetry, and UTF-8 locale
#  hardening for non-standard cluster images.
#
#  Current status: designed for cluster deployment, tested on desktop only.
#  Cluster-scale validation is a v0.7+ target.
#
#  Features
#  --------
#  1. Manual NUMA / MPI / OpenMP overrides — bypass auto-detection and
#     control process topology explicitly (useful on NUMA-aware nodes).
#
#  2. AMR subcycling telemetry — structured per-rank JSONL log of
#     parallel communication overhead during Adaptive Mesh Refinement,
#     for future scaling analysis on distributed-memory systems.
#
#  3. UTF-8 / locale hardening — pins the subprocess environment to
#     en_US.UTF-8 to prevent byte-encoding issues on non-standard images.
#
#  4. MPI launch integration — if --mpi-ranks > 1, the binary is
#     automatically wrapped with `mpirun` (or a custom scheduler launcher).
#
#  Usage (examples)
#  ----------------
#  # Minimal run (auto-detect everything):
#  python run_granite_hpc.py build/bin/granite_main benchmarks/B2_eq/params.yaml
#
#  # Manual thread + rank override:
#  python run_granite_hpc.py build/bin/granite_main benchmarks/B2_eq/params.yaml \
#      --omp-threads 32           \
#      --mpi-ranks 128            \
#      --disable-numa-bind        \
#      --amr-telemetry-file /scratch/$USER/amr_scaling.log
#
#  # Use a custom MPI launcher (e.g., srun on SLURM):
#  python run_granite_hpc.py build/bin/granite_main benchmarks/B2_eq/params.yaml \
#      --mpi-ranks 256            \
#      --mpi-launcher srun        \
#      --mpi-extra-args "--ntasks-per-node=4 --cpus-per-task=8"
#
# =============================================================================

from __future__ import annotations

import os
import re
import sys
import time
import shutil
import signal
import logging
import argparse
import datetime
import subprocess
from pathlib import Path
from typing import List, Optional

# ---------------------------------------------------------------------------
# 0.  UTF-8 HARDENING (Windows)
# ---------------------------------------------------------------------------
if sys.platform == "win32":
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8",
                                  errors="replace", line_buffering=True)
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8",
                                  errors="replace", line_buffering=True)

# ---------------------------------------------------------------------------
# 1.  LOGGING
# ---------------------------------------------------------------------------
logging.basicConfig(
    level=logging.INFO,
    format="[run_granite] %(levelname)s %(message)s",
    stream=sys.stderr,
)
log = logging.getLogger("run_granite")

# ---------------------------------------------------------------------------
# 2.  AMR SUBCYCLING TELEMETRY
# ---------------------------------------------------------------------------

class AMRTelemetryLogger:
    """
    Structured logger for AMR subcycling parallel communication overhead.

    Each step that triggers an AMR subcycle emits a JSON-serialisable record:
    {
        "wall_s"       : <float>,   # wall-clock seconds since job start
        "mpi_rank"     : <int>,     # MPI rank (0 if single-process)
        "amr_level"    : <int>,     # refinement level that subcycled
        "subcycle_ms"  : <float>,   # duration of the subcycle (ms, estimated)
        "blocks_synced": <int>,     # number of AMR blocks synced across ranks
        "raw_line"     : <str>      # verbatim engine output line (optional)
    }

    Usage
    -----
        tel = AMRTelemetryLogger(path=Path("/scratch/user/amr.log"), mpi_rank=0)
        tel.record(amr_level=2, subcycle_ms=14.7, blocks_synced=256,
                   raw_line="[AMR] subcycle level=2 sync=256 14.7ms")
        tel.close()
    """

    # Regex patterns to extract AMR subcycling data from engine stdout.
    # These are designed to match GRANITE's diagnostic output format.
    _RE_AMR_SUBCYCLE = re.compile(
        r"(?:\[AMR\].*subcycle|AMR:.*level.*subcycl)"
        r".*?level\s*=?\s*(\d+)"
        r"(?:.*?sync\s*=?\s*(\d+))?",
        re.IGNORECASE,
    )
    _RE_SUBCYCLE_MS = re.compile(r"([\d.]+)\s*ms", re.IGNORECASE)

    def __init__(self, path: Optional[Path], mpi_rank: int = 0) -> None:
        self._path     = path
        self._rank     = mpi_rank
        self._start    = time.monotonic()
        self._fh       = None

        if path is not None:
            path.parent.mkdir(parents=True, exist_ok=True)
            self._fh = open(path, "a", encoding="utf-8")
            self._fh.write(
                f"# GRANITE AMR Subcycling Telemetry — "
                f"started {datetime.datetime.now().isoformat()} "
                f"rank={mpi_rank}\n"
            )
            self._fh.flush()
            log.info("AMR telemetry → %s (rank %d)", path, mpi_rank)

    # ------------------------------------------------------------------
    def try_parse_and_record(self, line: str) -> bool:
        """
        Attempt to parse *line* as an AMR subcycle event.
        Returns True if a telemetry record was written.
        """
        m = self._RE_AMR_SUBCYCLE.search(line)
        if not m:
            return False

        amr_level    = int(m.group(1)) if m.group(1) else 0
        blocks_synced= int(m.group(2)) if m.group(2) else 0

        ms_m = self._RE_SUBCYCLE_MS.search(line)
        subcycle_ms  = float(ms_m.group(1)) if ms_m else 0.0

        self.record(
            amr_level=amr_level,
            subcycle_ms=subcycle_ms,
            blocks_synced=blocks_synced,
            raw_line=line.rstrip(),
        )
        return True

    # ------------------------------------------------------------------
    def record(self, amr_level: int, subcycle_ms: float,
               blocks_synced: int, raw_line: str = "") -> None:
        """Write one structured telemetry record."""
        if self._fh is None:
            return
        wall = time.monotonic() - self._start
        self._fh.write(
            f'{{"wall_s":{wall:.3f},'
            f'"mpi_rank":{self._rank},'
            f'"amr_level":{amr_level},'
            f'"subcycle_ms":{subcycle_ms:.3f},'
            f'"blocks_synced":{blocks_synced},'
            f'"raw_line":{repr(raw_line)}}}\n'
        )
        self._fh.flush()

    # ------------------------------------------------------------------
    def close(self) -> None:
        if self._fh and not self._fh.closed:
            self._fh.write(
                f"# ended {datetime.datetime.now().isoformat()}\n"
            )
            self._fh.close()

# ---------------------------------------------------------------------------
# 3.  ENVIRONMENT BUILDER
# ---------------------------------------------------------------------------

def build_env(
    omp_threads: Optional[int],
    disable_numa_bind: bool,
) -> dict:
    """
    Build the child-process environment dict.

    Rules (in priority order):
    1. If --omp-threads is given, OMP_NUM_THREADS is set to that value,
       OVERRIDING any value inherited from the parent shell.
    2. If --disable-numa-bind is given, GOMP_SPINCOUNT is set to 0,
       numactl is NOT prepended (handled in command builder), and
       KMP_AFFINITY / OMP_PROC_BIND are cleared to prevent the OpenMP
       runtime from attempting automatic NUMA pinning.
    3. UTF-8 locale is always pinned for Unicode output consistency.
    """
    env = os.environ.copy()

    # UTF-8 locale pin — prevents encoding corruption on exotic HPC images.
    env["LANG"]             = "en_US.UTF-8"
    env["LC_ALL"]           = "en_US.UTF-8"
    env["PYTHONIOENCODING"] = "utf-8"

    # OpenMP thread count override.
    if omp_threads is not None:
        env["OMP_NUM_THREADS"] = str(omp_threads)
        log.info("OMP_NUM_THREADS forced to %d", omp_threads)
    else:
        log.info("OMP_NUM_THREADS: using inherited value (%s)",
                 env.get("OMP_NUM_THREADS", "unset → runtime default"))

    # NUMA binding override.
    if disable_numa_bind:
        # Disable OpenMP NUMA affinity hooks completely.
        env.pop("KMP_AFFINITY",    None)   # Intel runtime
        env.pop("GOMP_CPU_AFFINITY", None) # GCC runtime
        env["OMP_PROC_BIND"]   = "false"
        env["GOMP_SPINCOUNT"]  = "0"
        log.info("NUMA binding DISABLED — OMP affinity cleared")
    else:
        log.info("NUMA binding: auto (inherited from environment)")

    return env

# ---------------------------------------------------------------------------
# 4.  COMMAND BUILDER
# ---------------------------------------------------------------------------

def build_command(
    binary: str,
    params: str,
    mpi_ranks: int,
    mpi_launcher: str,
    mpi_extra_args: str,
    disable_numa_bind: bool,
    numactl_args: Optional[str],
) -> List[str]:
    """
    Construct the final subprocess command list.

    Topology:
      numactl [numactl_args]  mpirun/srun -n N  granite_main  params.yaml

    Each component is optional:
      - numactl  : only if NOT --disable-numa-bind and numactl is present
      - mpirun   : only if mpi_ranks > 1
    """
    cmd: List[str] = []

    # ── numactl wrapper ────────────────────────────────────────────────
    if not disable_numa_bind:
        numactl = shutil.which("numactl")
        if numactl:
            cmd.append(numactl)
            if numactl_args:
                cmd.extend(numactl_args.split())
            else:
                cmd.append("--interleave=all")   # safe default
            log.info("NUMA wrapper: %s %s", numactl,
                     numactl_args or "--interleave=all")
        else:
            log.info("numactl not found — skipping NUMA binding")
    else:
        log.info("NUMA binding explicitly disabled by --disable-numa-bind")

    # ── MPI launcher ───────────────────────────────────────────────────
    if mpi_ranks > 1:
        launcher = shutil.which(mpi_launcher) or mpi_launcher
        cmd.extend([launcher, "-n", str(mpi_ranks)])
        if mpi_extra_args:
            cmd.extend(mpi_extra_args.split())
        log.info("MPI launcher: %s -n %d %s", launcher, mpi_ranks,
                 mpi_extra_args or "")
    else:
        log.info("Single-process mode (no MPI launcher)")

    # ── Binary + params ────────────────────────────────────────────────
    cmd.extend([binary, params])
    return cmd

# ---------------------------------------------------------------------------
# 5.  MAIN RUNNER
# ---------------------------------------------------------------------------

def run(args: argparse.Namespace) -> int:
    """
    Launch GRANITE with the requested hardware topology, stream its stdout,
    run AMR telemetry parsing, and return the exit code.
    """
    # Telemetry logger (None if --amr-telemetry-file not given).
    tel = AMRTelemetryLogger(
        path=Path(args.amr_telemetry_file) if args.amr_telemetry_file else None,
        mpi_rank=0,
    )

    # Build subprocess topology.
    cmd = build_command(
        binary          = args.binary,
        params          = args.params,
        mpi_ranks       = args.mpi_ranks,
        mpi_launcher    = args.mpi_launcher,
        mpi_extra_args  = args.mpi_extra_args or "",
        disable_numa_bind = args.disable_numa_bind,
        numactl_args    = args.numactl_args,
    )
    env = build_env(
        omp_threads       = args.omp_threads,
        disable_numa_bind = args.disable_numa_bind,
    )

    log.info("Command: %s", " ".join(cmd))

    if getattr(args, "slurm", False):
        log.info("Generating SLURM submission script...")
        jobs_dir = Path("jobs")
        jobs_dir.mkdir(exist_ok=True)
        script_path = jobs_dir / "submit_granite.sbatch"
        nodes = max(1, args.mpi_ranks // 32)
        omp = args.omp_threads or 1
        
        env_lines = []
        for k, v in env.items():
            if k in ("OMP_NUM_THREADS", "OMP_PROC_BIND", "GOMP_SPINCOUNT", "LANG", "LC_ALL"):
                env_lines.append(f"export {k}={v}")
                
        cmd_str = " ".join(cmd)
        content = f"""#!/bin/bash
#SBATCH --job-name=granite_sim
#SBATCH --nodes={nodes}
#SBATCH --ntasks={args.mpi_ranks}
#SBATCH --cpus-per-task={omp}
#SBATCH --time=24:00:00
#SBATCH --output=granite_sim_%j.log
#SBATCH --error=granite_sim_%j.log

{"\n".join(env_lines)}

export PYTHONPATH="$(pwd)/python:$PYTHONPATH"

{cmd_str} | python -m granite_analysis.cli.sim_tracker
"""
        script_path.write_text(content, encoding="utf-8")
        log.info(f"SLURM script generated at {script_path.absolute()}")
        log.info("Submitting via sbatch...")
        try:
            ret = subprocess.run(["sbatch", str(script_path)])
            return ret.returncode
        except FileNotFoundError:
            log.warning("'sbatch' not found — script generated but NOT submitted.")
            log.info("To submit manually on the cluster, run:  sbatch %s", script_path)
            return 0

    # ── Launch ─────────────────────────────────────────────────────────
    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=1,
            universal_newlines=True,
            encoding="utf-8",
            errors="replace",
            env=env,
        )
    except FileNotFoundError as e:
        log.error("Binary not found: %s", e)
        tel.close()
        return 1

    # ── Stream stdout, parse AMR telemetry ─────────────────────────────
    interrupted = False

    def _sigint(signum, frame):  # noqa: ANN001
        nonlocal interrupted
        interrupted = True
        proc.terminate()

    signal.signal(signal.SIGINT, _sigint)

    try:
        for line in proc.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            # Feed every line to the telemetry parser.
            tel.try_parse_and_record(line)
            if interrupted:
                break
    except Exception as exc:
        log.error("Stream error: %s", exc)
    finally:
        proc.wait()
        tel.close()

    ret = proc.returncode or 0
    if interrupted:
        log.info("Run interrupted (SIGINT) at user request.")
        ret = 0

    log.info("GRANITE exited with code %d", ret)
    return ret

# ---------------------------------------------------------------------------
# 6.  CLI
# ---------------------------------------------------------------------------

def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="run_granite.py",
        description=(
            "GRANITE HPC launch wrapper — manual NUMA/MPI overrides, "
            "AMR subcycling telemetry, and UTF-8 environment pinning."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
examples:
  # Auto-detect everything (simplest):
  python run_granite.py build/bin/granite_main benchmarks/B2_eq/params.yaml

  # Full manual HPC admin override:
  python run_granite.py build/bin/granite_main benchmarks/B2_eq/params.yaml \\
      --omp-threads 32 --mpi-ranks 128 --disable-numa-bind \\
      --amr-telemetry-file /scratch/$USER/amr.log

  # Custom SLURM launcher:
  python run_granite.py build/bin/granite_main benchmarks/B2_eq/params.yaml \\
      --mpi-ranks 256 --mpi-launcher srun \\
      --mpi-extra-args "--ntasks-per-node=4 --cpus-per-task=8"
""",
    )

    # Positional
    p.add_argument("binary",
                   help="Path to the granite_main binary.")
    p.add_argument("params",
                   help="Path to the params.yaml configuration file.")

    # OpenMP / threading
    t = p.add_argument_group("OpenMP / Threading")
    t.add_argument("--omp-threads", type=int, default=None, metavar="N",
                   help="Force OMP_NUM_THREADS=N, overriding auto-detection.")

    # NUMA
    n = p.add_argument_group("NUMA Binding")
    n.add_argument("--disable-numa-bind", action="store_true",
                   help="Disable ALL NUMA/CPU affinity binding. "
                        "numactl is NOT invoked; OMP_PROC_BIND is cleared.")
    n.add_argument("--numactl-args", default=None, metavar="ARGS",
                   help="Custom numactl arguments (default: --interleave=all). "
                        "Ignored if --disable-numa-bind is set.")

    # MPI
    m = p.add_argument_group("MPI")
    m.add_argument("--mpi-ranks", type=int, default=1, metavar="N",
                   help="Number of MPI ranks. If >1, wraps binary with "
                        "the MPI launcher. Default: 1 (single process).")
    m.add_argument("--mpi-launcher", default="mpirun", metavar="CMD",
                   help="MPI launcher executable (default: mpirun). "
                        "Use 'srun' on SLURM, 'aprun' on Cray, etc.")
    m.add_argument("--mpi-extra-args", default=None, metavar="ARGS",
                   help="Extra arguments forwarded to the MPI launcher "
                        "(e.g. '--ntasks-per-node 4 --cpus-per-task 8').")

    # Telemetry
    tel = p.add_argument_group("AMR Subcycling Telemetry")
    tel.add_argument("--amr-telemetry-file", default=None, metavar="PATH",
                     help="Write structured AMR subcycling telemetry (JSONL) "
                          "to this file. Each line is a JSON record containing "
                          "wall time, MPI rank, AMR level, sync duration, and "
                          "block count. Ideal for post-run scaling analysis.")

    # SLURM
    s = p.add_argument_group("SLURM Integration")
    s.add_argument("--slurm", action="store_true",
                   help="Generate a SLURM .sbatch script and submit it, piping output to sim_tracker.")

    return p


def main() -> None:
    parser = _build_parser()
    args   = parser.parse_args()

    if not getattr(args, "slurm", False) and not Path(args.binary).exists():
        log.error("Binary does not exist: %s", args.binary)
        sys.exit(1)

    sys.exit(run(args))


if __name__ == "__main__":
    main()
