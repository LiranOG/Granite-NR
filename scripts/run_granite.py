#!/usr/bin/env python3
"""
GRANITE Developer Workflow Tool
---------------------------------
A professional CLI for the day-to-day development cycle of the GRANITE engine.
Covers the full loop: build → run → test → clean → format.

For HPC cluster deployments (MPI ranks, NUMA binding, AMR telemetry),
use scripts/run_granite_hpc.py instead.

Usage:
  python scripts/run_granite.py [--verbose] [--dry-run] <command> [options]

Commands:
  build   [--build-type TYPE] [--flag K=V ...]   Configure & compile with CMake
  run     --benchmark NAME [--omp-threads N]      Run a simulation locally
  test    [--filter REGEX] [--timeout S]          Run ctest
  clean   [--build-dir PATH]                      Remove build artifacts
  format  [--check]                               Auto-format with clang-format-18
"""

from __future__ import annotations

import argparse
import logging
import os
import re
import shlex
import shutil
import signal
import subprocess
import sys
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# Colour-coded logging  (degrades gracefully on non-TTY / CI pipes)
# ---------------------------------------------------------------------------
_IS_TTY = sys.stderr.isatty()

_C = {
    "reset":  "\033[0m"  if _IS_TTY else "",
    "bold":   "\033[1m"  if _IS_TTY else "",
    "grey":   "\033[90m" if _IS_TTY else "",
    "green":  "\033[32m" if _IS_TTY else "",
    "yellow": "\033[33m" if _IS_TTY else "",
    "red":    "\033[31m" if _IS_TTY else "",
    "cyan":   "\033[36m" if _IS_TTY else "",
}

_LEVEL_COLOUR = {
    logging.DEBUG:    _C["grey"],
    logging.INFO:     _C["green"],
    logging.WARNING:  _C["yellow"],
    logging.ERROR:    _C["red"],
    logging.CRITICAL: _C["red"] + _C["bold"],
}


class _Formatter(logging.Formatter):
    def format(self, record: logging.LogRecord) -> str:
        ts    = self.formatTime(record, "%H:%M:%S")
        lvl   = record.levelname.ljust(8)
        msg   = record.getMessage()
        col   = _LEVEL_COLOUR.get(record.levelno, "")
        tag   = f"{_C['cyan']}[granite]{_C['reset']}"
        return (
            f"{_C['grey']}{ts}{_C['reset']} "
            f"{col}{_C['bold']}{lvl}{_C['reset']} "
            f"{tag} {msg}"
        )


def _setup_logging(verbose: bool) -> None:
    h = logging.StreamHandler(sys.stderr)
    h.setFormatter(_Formatter())
    root = logging.getLogger()
    root.handlers.clear()
    root.addHandler(h)
    root.setLevel(logging.DEBUG if verbose else logging.INFO)


log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Global flags (set once in main())
# ---------------------------------------------------------------------------
DRY_RUN = False

# ---------------------------------------------------------------------------
# Physical-core auto-detection for OMP_NUM_THREADS
# ---------------------------------------------------------------------------
_FALLBACK_CORES = 4


def _physical_cores() -> tuple[int, str]:
    """Return (count, method_description) — never raises."""

    # Method 1: psutil — most accurate (physical only, cross-platform)
    try:
        import psutil
        n = psutil.cpu_count(logical=False)
        if n and n > 0:
            return n, "psutil"
    except ImportError:
        pass

    # Method 2: os.cpu_count() — returns logical cores, still beats the
    # hardcoded fallback on any reasonably modern machine.
    n = os.cpu_count()
    if n and n > 0:
        return n, "os.cpu_count [logical]"

    # Method 3: platform-native query (no bash required)
    try:
        if sys.platform == "darwin":
            r = subprocess.run(["sysctl", "-n", "hw.physicalcpu"],
                               capture_output=True, text=True, timeout=5)
            n = int(r.stdout.strip())
            if n > 0:
                return n, "sysctl"
        else:
            # Read /proc/cpuinfo directly — no bash, no grep, no wc.
            cpuinfo = Path("/proc/cpuinfo")
            if cpuinfo.exists():
                ids = {line.split(":")[1].strip()
                       for line in cpuinfo.read_text(errors="replace").splitlines()
                       if line.startswith("core id")}
                n = len(ids)
                if n > 0:
                    return n, "/proc/cpuinfo"
    except Exception:
        pass

    return _FALLBACK_CORES, "fallback"


def _apply_omp(env: dict, forced: Optional[int]) -> dict:
    if forced is not None:
        env["OMP_NUM_THREADS"] = str(forced)
        log.info("OMP_NUM_THREADS = %d  (--omp-threads)", forced)
    elif "OMP_NUM_THREADS" not in env:
        n, method = _physical_cores()
        env["OMP_NUM_THREADS"] = str(n)
        env.setdefault("OMP_PROC_BIND", "close")
        env.setdefault("OMP_PLACES",    "cores")
        log.info("OMP_NUM_THREADS = %d  (auto-detected via %s)", n, method)
    else:
        log.debug("OMP_NUM_THREADS = %s  (inherited)", env["OMP_NUM_THREADS"])
    return env


# ---------------------------------------------------------------------------
# Lightweight dependency checks
# ---------------------------------------------------------------------------
def _need(cmd: str, hint: str = "") -> str:
    """Return the full path to *cmd*, or exit with a clear message."""
    path = shutil.which(cmd)
    if path is None:
        log.error("'%s' not found in PATH.%s", cmd, f"  {hint}" if hint else "")
        sys.exit(2)
    return path


def _cmake_path() -> str:
    return _need("cmake", "Install CMake >= 3.18 from https://cmake.org/download/")


def _ctest_path() -> str:
    # ctest ships with cmake
    return _need("ctest", "ctest is bundled with CMake — reinstall CMake.")


def _clang_format_path() -> str:
    """Prefer clang-format-18; warn if a different version is found."""
    for name in ("clang-format-18", "clang-format"):
        p = shutil.which(name)
        if p is None:
            continue
        try:
            raw = subprocess.check_output([p, "--version"],
                                          stderr=subprocess.STDOUT, text=True)
            ver_match = re.search(r"(\d+)\.\d+", raw)
            ver_major = int(ver_match.group(1)) if ver_match else 0
            if ver_major != 18:
                log.warning(
                    "Found clang-format version %d — project targets 18. "
                    "Output may diverge from CI.", ver_major
                )
        except Exception:
            pass
        return p
    log.error("clang-format not found. Install clang-format-18.")
    sys.exit(2)


# ---------------------------------------------------------------------------
# Safe subprocess runner with Ctrl+C handling
# ---------------------------------------------------------------------------
_proc: Optional[subprocess.Popen] = None


def _sigint(_sig, _frame):
    """Kill the active child process group on Ctrl+C.

    Two key fixes vs. the naive approach:
    1. signal.SIG_IGN is installed immediately so repeated Ctrl+C presses
       cannot re-enter this handler or spam EINTR into _proc.wait(), which
       would prevent the 5-second timeout from ever expiring.
    2. os.killpg() targets the child's entire process group rather than just
       the direct child PID, so grandchildren (e.g. piped analysis tools)
       are also reaped cleanly.
    """
    global _proc
    signal.signal(signal.SIGINT, signal.SIG_IGN)   # block re-entry
    if _proc is not None:
        pid = _proc.pid
        log.warning("Interrupted — terminating child process group (PID %d)…", pid)
        try:
            os.killpg(os.getpgid(pid), signal.SIGTERM)
        except (ProcessLookupError, PermissionError, OSError):
            pass  # already dead
        try:
            _proc.wait(timeout=1)
        except subprocess.TimeoutExpired:
            log.warning("Child did not exit cleanly — sending SIGKILL…")
            try:
                os.killpg(os.getpgid(pid), signal.SIGKILL)
            except (ProcessLookupError, PermissionError, OSError):
                pass
            _proc.wait()
    log.error("Aborted by user.")
    sys.exit(130)


def run(cmd: list[str], env: Optional[dict] = None,
        cwd: Optional[Path] = None) -> int:
    """Execute *cmd*, propagating its exit code.  Respects DRY_RUN."""
    global _proc
    display = shlex.join(str(c) for c in cmd)
    if DRY_RUN:
        log.info("[dry-run] %s", display)
        return 0
    log.debug("$ %s", display)
    prev = signal.signal(signal.SIGINT, _sigint)
    try:
        _proc = subprocess.Popen(cmd, env=env, cwd=cwd,
                                 start_new_session=True)  # own pgid → Ctrl+C reaches Python only
        return _proc.wait()
    finally:
        _proc = None
        signal.signal(signal.SIGINT, prev)


# ---------------------------------------------------------------------------
# Sub-commands
# ---------------------------------------------------------------------------

def cmd_build(args) -> None:
    cmake = _cmake_path()
    build_dir = Path(args.build_dir)
    build_dir.mkdir(parents=True, exist_ok=True)

    build_type = args.build_type
    log.info("Configuring  [%s]  →  %s/", build_type, build_dir)

    config = [cmake, "-B", str(build_dir), "-S", ".",
               f"-DCMAKE_BUILD_TYPE={build_type}"]

    for flag in args.flag or []:
        config.append(f"-D{flag}" if not flag.startswith("-D") else flag)
        log.debug("  cmake flag: %s", flag)

    rc = run(config)
    if rc != 0:
        log.error("CMake configuration failed (exit %d).", rc)
        sys.exit(rc)

    jobs = str(os.cpu_count() or 4)
    log.info("Compiling  (-j%s)…", jobs)
    rc = run([cmake, "--build", str(build_dir), f"-j{jobs}"])
    if rc != 0:
        log.error("Compilation failed (exit %d).", rc)
        sys.exit(rc)

    log.info("Build complete.")


def cmd_run(args) -> None:
    build_dir = Path(args.build_dir)
    suffix    = ".exe" if sys.platform == "win32" else ""
    candidates = [
        build_dir / "bin"     / f"granite_main{suffix}",
        build_dir / "bin" / "Release" / f"granite_main{suffix}",
        build_dir / "Release" / f"granite_main{suffix}",
        build_dir             / f"granite_main{suffix}",
    ]
    exe = next((c for c in candidates if c.exists()), None)
    if exe is None:
        log.error("Executable not found. Searched:\n  %s",
                  "\n  ".join(str(c) for c in candidates))
        log.error("Did you run 'build' first?")
        sys.exit(1)

    benchmark_dir = Path("benchmarks") / args.benchmark
    param_file    = benchmark_dir / "params.yaml"

    if not benchmark_dir.exists():
        log.error("Benchmark directory not found: %s", benchmark_dir)
        sys.exit(1)
    if not param_file.exists():
        log.warning("params.yaml not found in %s — running with compiled defaults.",
                    benchmark_dir)

    sim_cmd = [str(exe)]
    if param_file.exists():
        sim_cmd.append(str(param_file))

    env = _apply_omp(os.environ.copy(), args.omp_threads)

    log.info("Launching  %s  [OMP_NUM_THREADS=%s]",
             " ".join(sim_cmd), env.get("OMP_NUM_THREADS", "?"))
    rc = run(sim_cmd, env=env)
    if rc != 0:
        log.error("Simulation exited with code %d.", rc)
    sys.exit(rc)


def cmd_test(args) -> None:
    ctest    = _ctest_path()
    build_dir = Path(args.build_dir)

    if not (build_dir / "CTestTestfile.cmake").exists():
        log.error("No CTest files in '%s'. Run 'build' with tests enabled first.",
                  build_dir)
        sys.exit(1)

    ctest_cmd = [
        ctest,
        "--test-dir", str(build_dir),
        "--output-on-failure",
        "--timeout",  str(args.timeout),
    ]
    if args.filter:
        ctest_cmd += ["-R", args.filter]

    log.info("Running test suite%s…",
             f"  [filter: {args.filter}]" if args.filter else "")
    rc = run(ctest_cmd)
    if rc != 0:
        log.error("Tests FAILED (ctest exit %d).", rc)
    else:
        log.info("All tests passed.")
    sys.exit(rc)


def cmd_clean(args) -> None:
    build_dir = Path(args.build_dir)
    if not build_dir.exists():
        log.info("Nothing to clean ('%s' does not exist).", build_dir)
        return
    if DRY_RUN:
        log.info("[dry-run] Would remove: %s", build_dir)
        return
    log.info("Removing %s/…", build_dir)
    try:
        shutil.rmtree(build_dir)
        log.info("Clean complete.")
    except PermissionError as e:
        log.error("Permission denied: %s", e)
        sys.exit(1)
    except OSError as e:
        log.error("Could not remove '%s': %s", build_dir, e)
        sys.exit(1)


def cmd_format(args) -> None:
    formatter = _clang_format_path()

    sources = [
        f
        for d in (Path("src"), Path("include"), Path("tests"))
        if d.exists()
        for ext in ("*.cpp", "*.hpp", "*.c", "*.h")
        for f in d.rglob(ext)
    ]

    if not sources:
        log.warning("No source files found.")
        return

    action = "Checking" if args.check else "Formatting"
    log.info("%s %d file(s) with %s…", action, len(sources),
             Path(formatter).name)

    flags = ["--dry-run", "--Werror"] if args.check else ["-i"]

    # Pass all files in one subprocess call — ~10x faster than per-file loop.
    # xargs-style: if the arg list exceeds ARG_MAX we chunk it.
    _MAX_BATCH = 200
    failed_files: list[Path] = []
    for i in range(0, len(sources), _MAX_BATCH):
        batch = sources[i : i + _MAX_BATCH]
        rc    = run([formatter] + flags + [str(f) for f in batch])
        if rc != 0 and args.check:
            # Re-run individually to identify which files failed.
            failed_files += [f for f in batch
                             if run([formatter] + flags + [str(f)]) != 0]
    bad = failed_files

    if args.check and bad:
        log.error("%d file(s) need formatting:", len(bad))
        for f in bad:
            log.error("  %s", f)
        sys.exit(1)
    elif not args.check:
        log.info("Formatted %d file(s).", len(sources))


def cmd_dev(args) -> None:
    import time
    def get_latest_mtime() -> float:
        mtime = 0.0
        for d in (Path("src"), Path("include")):
            if d.exists():
                for ext in ("*.cpp", "*.hpp", "*.c", "*.h"):
                    for f in d.rglob(ext):
                        mtime = max(mtime, f.stat().st_mtime)
        return mtime

    def run_pipeline() -> tuple[subprocess.Popen, subprocess.Popen]:
        # Always build first
        build_args = argparse.Namespace(build_dir="build", build_type="Release", flag=[])
        try:
            cmd_build(build_args)
        except SystemExit as e:
            if e.code != 0:
                log.error("Build failed, waiting for changes...")
                return None, None

        suffix = ".exe" if sys.platform == "win32" else ""
        candidates = [
            Path("build/bin") / f"granite_main{suffix}",
            Path("build/bin/Release") / f"granite_main{suffix}",
            Path("build/Release") / f"granite_main{suffix}",
            Path("build") / f"granite_main{suffix}",
        ]
        exe = next((c for c in candidates if c.exists()), None)
        if not exe:
            log.error("Executable not found after build!")
            sys.exit(1)

        param_file = Path("benchmarks") / "single_puncture" / "params.yaml"
        sim_cmd = [str(exe)]
        if param_file.exists():
            sim_cmd.append(str(param_file))

        env = _apply_omp(os.environ.copy(), None)
        env["PYTHONPATH"] = str(Path("python").absolute())
        
        log.info("Launching dev pipeline...")
        sim_proc = subprocess.Popen(sim_cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        dev_proc = subprocess.Popen([sys.executable, "-m", "granite_analysis.cli.dev_benchmark"], stdin=sim_proc.stdout, env=env)
        return sim_proc, dev_proc

    sim_proc, dev_proc = run_pipeline()

    if args.watch:
        last_mtime = get_latest_mtime()
        try:
            while True:
                time.sleep(1.0)
                current_mtime = get_latest_mtime()
                if current_mtime > last_mtime:
                    log.info("Source change detected. Recompiling and restarting...")
                    if sim_proc: sim_proc.terminate()
                    if dev_proc: dev_proc.terminate()
                    if sim_proc: sim_proc.wait()
                    if dev_proc: dev_proc.wait()
                    last_mtime = current_mtime
                    sim_proc, dev_proc = run_pipeline()
        except KeyboardInterrupt:
            if sim_proc: sim_proc.terminate()
            if dev_proc: dev_proc.terminate()
            sys.exit(0)
    else:
        try:
            if dev_proc: dev_proc.wait()
            if sim_proc: sim_proc.wait()
        except KeyboardInterrupt:
            if sim_proc: sim_proc.terminate()
            if dev_proc: dev_proc.terminate()
            sys.exit(0)


def cmd_docs(args) -> None:
    docs_src = Path("docs")
    docs_out = Path("docs/_build/html")
    sphinx = shutil.which("sphinx-build")
    if not sphinx:
        log.error(
            "sphinx-build not found. Install docs dependencies: "
            "pip install -e .[docs]"
        )
        sys.exit(1)
    _run([sphinx, str(docs_src), str(docs_out), "-b", "html"])
    log.info("Docs built → %s/index.html", docs_out)
    if getattr(args, "open", False):
        import webbrowser
        webbrowser.open((docs_out / "index.html").as_uri())


# ---------------------------------------------------------------------------
# Argument parser
# ---------------------------------------------------------------------------
def _build_parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(
        prog="run_granite.py",
        description="GRANITE developer workflow tool  "
                    "(HPC/cluster deployments -> run_granite_hpc.py)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "examples:\n"
            "  python scripts/run_granite.py build\n"
            "  python scripts/run_granite.py build --build-type Debug --flag GRANITE_ENABLE_TESTS=ON\n"
            "  python scripts/run_granite.py run --benchmark single_puncture\n"
            "  python scripts/run_granite.py test --filter smoke\n"
            "  python scripts/run_granite.py clean\n"
            "  python scripts/run_granite.py format\n"
            "  python scripts/run_granite.py format --check\n"
            "  python scripts/run_granite.py --dry-run build\n"
        ),
    )
    root.add_argument("--verbose", "-v", action="store_true",
                      help="Show DEBUG-level log messages")
    root.add_argument("--dry-run", "-n", action="store_true",
                      help="Print commands without executing them")

    subs = root.add_subparsers(dest="command", metavar="<command>")
    subs.required = True

    # ── build ─────────────────────────────────────────────────────────────
    pb = subs.add_parser("build", help="Configure and compile with CMake")
    pb.add_argument("--build-dir",   default="build",   metavar="PATH",
                    help="Build directory (default: build/)")
    pb.add_argument("--build-type",  default="Release",
                    choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"],
                    help="CMake build type (default: Release)")
    pb.add_argument("--flag", action="append", metavar="KEY=VALUE",
                    help="Pass -DKEY=VALUE to CMake (repeatable). "
                         "E.g. --flag GRANITE_ENABLE_TESTS=ON")

    # ── run ───────────────────────────────────────────────────────────────
    pr = subs.add_parser("run", help="Run a simulation benchmark locally")
    pr.add_argument("--build-dir",   default="build",   metavar="PATH")
    pr.add_argument("--benchmark",   required=True,     metavar="NAME",
                    help="Benchmark directory under benchmarks/ "
                         "(e.g. single_puncture, B2_eq)")
    pr.add_argument("--omp-threads", type=int, default=None, metavar="N",
                    help="Force OMP_NUM_THREADS (default: auto-detect cores)")

    # ── test ──────────────────────────────────────────────────────────────
    pt = subs.add_parser("test", help="Run the test suite via ctest")
    pt.add_argument("--build-dir",  default="build",  metavar="PATH")
    pt.add_argument("--filter",     default=None,     metavar="REGEX",
                    help="Run only tests matching this regex (-R)")
    pt.add_argument("--timeout",    type=int, default=120, metavar="S",
                    help="Per-test timeout in seconds (default: 120)")

    # ── clean ─────────────────────────────────────────────────────────────
    pc = subs.add_parser("clean", help="Remove build artifacts")
    pc.add_argument("--build-dir",  default="build",  metavar="PATH")

    # ── format ────────────────────────────────────────────────────────────
    pf = subs.add_parser("format", help="Format C++ sources with clang-format-18")
    pf.add_argument("--check", action="store_true",
                    help="Check only — exit non-zero if any file needs reformatting")

    # ── dev ───────────────────────────────────────────────────────────────
    pd = subs.add_parser("dev", help="Run the default benchmark and pipe to dev_benchmark")
    pd.add_argument("--watch", action="store_true",
                    help="Watch src/ and include/ for changes, automatically rebuild and restart")

    # ── docs ──────────────────────────────────────────────────────────────
    pdoc = subs.add_parser("docs", help="Build Sphinx HTML documentation")
    pdoc.add_argument("--open", action="store_true",
                      help="Open the built docs in the default browser after building")

    return root


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main() -> None:
    parser = _build_parser()
    args   = parser.parse_args()

    _setup_logging(args.verbose)

    global DRY_RUN
    DRY_RUN = args.dry_run
    if DRY_RUN:
        log.info("[dry-run] Commands will be printed, not executed.")

    {
        "build":  cmd_build,
        "run":    cmd_run,
        "test":   cmd_test,
        "clean":  cmd_clean,
        "format": cmd_format,
        "dev":    cmd_dev,
        "docs":   cmd_docs,
    }[args.command](args)


if __name__ == "__main__":
    main()
