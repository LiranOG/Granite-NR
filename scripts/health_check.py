#!/usr/bin/env python3
"""
GRANITE Diagnostics & Health Check
-----------------------------------
Performs a full environment validation for the GRANITE numerical relativity engine.
Checks thread saturation (physical cores), CMake build flags, and hardware linkage.
"""

import os
import sys
import platform
from pathlib import Path

# ---------------------------------------------------------------------------
# Terminal coloring codes
# ---------------------------------------------------------------------------
class Colors:
    GREEN  = "\033[92m"
    YELLOW = "\033[93m"
    RED    = "\033[91m"
    CYAN   = "\033[96m"
    BLUE   = "\033[94m"
    RESET  = "\033[0m"
    BOLD   = "\033[1m"

# Enable ANSI on Windows (needed for cmd.exe / older PowerShell versions)
if sys.platform == "win32":
    import ctypes
    try:
        kernel32 = ctypes.windll.kernel32
        kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
    except Exception:
        pass  # Graceful fallback — ANSI will just be invisible


# ---------------------------------------------------------------------------
# Physical core detection  (cross-platform, zero-failure)
# ---------------------------------------------------------------------------
FALLBACK_CORES = 4

def detect_physical_cores() -> tuple[int, str]:
    """
    Returns (physical_core_count, method_used).

    Priority:
      1. psutil.cpu_count(logical=False)   — most accurate, cross-platform
      2. os.cpu_count(logical=False)       — Python 3.13+ (logical=False kwarg)
      3. platform-specific shell commands  — Windows WMIC / Linux nproc / macOS sysctl
      4. Hard fallback  → 4 cores + warning
    """

    # --- Method 1: psutil (preferred) ---
    try:
        import psutil
        cores = psutil.cpu_count(logical=False)
        if cores and cores > 0:
            return cores, "psutil"
    except ImportError:
        pass

    # --- Method 2: Python 3.13+ os.cpu_count(logical=False) ---
    try:
        cores = os.cpu_count(logical=False)  # type: ignore[call-arg]
        if cores and cores > 0:
            return cores, "os.cpu_count(logical=False)"
    except TypeError:
        pass

    # --- Method 3: Platform-specific shell commands ---
    try:
        import subprocess
        plat = sys.platform
        if plat == "win32":
            result = subprocess.run(
                ["powershell", "-Command",
                 "(Get-CimInstance Win32_Processor | Measure-Object -Property NumberOfCores -Sum).Sum"],
                capture_output=True, text=True, timeout=5
            )
            cores = int(result.stdout.strip())
        elif plat == "darwin":
            result = subprocess.run(
                ["sysctl", "-n", "hw.physicalcpu"],
                capture_output=True, text=True, timeout=5
            )
            cores = int(result.stdout.strip())
        else:  # Linux / WSL2
            result = subprocess.run(
                ["bash", "-c",
                 "grep '^core id' /proc/cpuinfo | sort -u | wc -l"],
                capture_output=True, text=True, timeout=5
            )
            cores = int(result.stdout.strip())

        if cores and cores > 0:
            return cores, f"shell ({plat})"
    except Exception:
        pass

    # --- Fallback ---
    print(
        f"\n   {Colors.YELLOW}⚠  WARNING: Physical core detection failed. "
        f"Defaulting to safe fallback of {FALLBACK_CORES} threads.{Colors.RESET}"
    )
    return FALLBACK_CORES, "fallback (detection failed)"


def shell_set_command(cores: int) -> str:
    """Return a ready-to-paste shell command for the current OS."""
    plat = sys.platform
    if plat == "win32":
        return (
            f'$env:OMP_NUM_THREADS = {cores}; '
            f'$env:OMP_PROC_BIND = "true"; '
            f'$env:OMP_PLACES = "cores"'
        )
    else:  # Linux / WSL2 / macOS
        return (
            f"export OMP_NUM_THREADS={cores}\n"
            f"export OMP_PROC_BIND=true\n"
            f"export OMP_PLACES=cores"
        )


# ---------------------------------------------------------------------------
# Reporting helpers
# ---------------------------------------------------------------------------
def print_header():
    print(f"\n{Colors.BOLD}{Colors.CYAN}=================================================={Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN}       GRANITE Diagnostics & Health Check         {Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN}=================================================={Colors.RESET}")
    # Show detected OS + Python version for context
    py  = sys.version.split()[0]
    os_ = f"{platform.system()} {platform.release()}"
    print(f"   {Colors.BLUE}Platform: {os_}   Python: {py}{Colors.RESET}")


def report(issue: str, status: str, ok: bool = True, warn: bool = False):
    if warn:
        color = Colors.YELLOW
        icon  = "[WARN]"
    elif ok:
        color = Colors.GREEN
        icon  = "[ OK ]"
    else:
        color = Colors.RED
        icon  = "[FAIL]"
    print(f" {Colors.BOLD}{issue.ljust(36)}{Colors.RESET} {color}{icon} {status}{Colors.RESET}")


# ---------------------------------------------------------------------------
# Task 1 — Smart thread saturation check
# ---------------------------------------------------------------------------
def check_openmp():
    logical_cores  = os.cpu_count() or FALLBACK_CORES
    physical_cores, method = detect_physical_cores()
    env_val        = os.environ.get("OMP_NUM_THREADS", "NOT_SET")

    report("Logical  Cores (HT/SMT included)", f"{logical_cores} detected")
    report("Physical Cores (true compute)",    f"{physical_cores} detected  [via {method}]")

    print()

    if env_val == "NOT_SET":
        report("OMP_NUM_THREADS", "NOT SET — will use OS default (sub-optimal)", ok=False)

        shell_hint = shell_set_command(physical_cores)
        print(
            f"\n   {Colors.YELLOW}{'─'*56}{Colors.RESET}\n"
            f"   {Colors.BOLD}{Colors.YELLOW}⚡ ACTION REQUIRED — Paste this into your terminal:{Colors.RESET}\n"
        )
        for line in shell_hint.splitlines():
            print(f"   {Colors.CYAN}  {line}{Colors.RESET}")
        print(f"\n   {Colors.YELLOW}{'─'*56}{Colors.RESET}")
        print(
            f"   {Colors.YELLOW}ℹ  Physical cores ({physical_cores}) avoids Hyper-Threading overhead,\n"
            f"      which causes false sharing in CCZ4 derivative stencils.{Colors.RESET}\n"
        )

    else:
        try:
            val = int(env_val)
            if val == physical_cores:
                report("OMP_NUM_THREADS", f"{val} ✓ Optimal (matches physical cores)", ok=True)
            elif val < physical_cores:
                report("OMP_NUM_THREADS", f"{val} — Under-saturating ({physical_cores} physical cores available)", warn=True)
            elif val <= logical_cores:
                report("OMP_NUM_THREADS", f"{val} — Using HT threads (may degrade GRMHD perf)", warn=True)
                print(
                    f"   {Colors.YELLOW}ℹ  Recommended: set to physical cores ({physical_cores}), "
                    f"not logical ({logical_cores}).{Colors.RESET}"
                )
            else:
                report("OMP_NUM_THREADS", f"{val} — Over-subscribed (> {logical_cores} logical cores!)", ok=False)
        except ValueError:
            report("OMP_NUM_THREADS", f"Invalid value: '{env_val}'", ok=False)

    # Additional env checks
    proc_bind = os.environ.get("OMP_PROC_BIND", "NOT_SET")
    places    = os.environ.get("OMP_PLACES",    "NOT_SET")

    if proc_bind == "NOT_SET":
        report("OMP_PROC_BIND", "Not set (thread migration allowed)", warn=True)
    else:
        report("OMP_PROC_BIND", proc_bind, ok=True)

    if places == "NOT_SET":
        report("OMP_PLACES", "Not set (no NUMA pinning)", warn=True)
    else:
        report("OMP_PLACES", places, ok=True)


# ---------------------------------------------------------------------------
# CMake cache scanner
# ---------------------------------------------------------------------------
def scan_cmake_cache():
    cache_path = Path("build/CMakeCache.txt")
    if not cache_path.exists():
        print(
            f"\n{Colors.RED}❌  ERROR: CMakeCache.txt not found in build/.\n"
            f"    Have you compiled the engine?  Run: python scripts/run_granite.py build{Colors.RESET}"
        )
        sys.exit(1)

    print(f"\n{Colors.BOLD}--- Compiling Flags & Pipeline ---{Colors.RESET}")

    with open(cache_path, "r") as f:
        content = f.read()

    build_type = "UNKNOWN"
    mpi        = False
    openmp     = False
    cxx_flags  = ""
    compiler   = "UNKNOWN"

    for line in content.splitlines():
        if   line.startswith("CMAKE_BUILD_TYPE:STRING="):
            build_type = line.split("=", 1)[1].strip()
        elif line.startswith("GRANITE_ENABLE_MPI:BOOL="):
            mpi = line.split("=", 1)[1].strip() == "ON"
        elif line.startswith("GRANITE_ENABLE_OPENMP:BOOL="):
            openmp = line.split("=", 1)[1].strip() == "ON"
        elif line.startswith("CMAKE_CXX_FLAGS_RELEASE:STRING="):
            cxx_flags = line.split("=", 1)[1].strip()
        elif line.startswith("CMAKE_CXX_COMPILER_ID:STRING="):
            compiler = line.split("=", 1)[1].strip()

    if build_type.upper() == "RELEASE":
        report("CMAKE_BUILD_TYPE", "Release (Maximum Power)", ok=True)
    elif build_type.upper() == "DEBUG":
        report("CMAKE_BUILD_TYPE", "DEBUG — ~40× performance penalty!", ok=False)
        print(f"   {Colors.RED}>> FATAL: Rebuild with -DCMAKE_BUILD_TYPE=Release!{Colors.RESET}")
    else:
        report("CMAKE_BUILD_TYPE", build_type, ok=False)

    report("C++ Compiler Engine", compiler, ok=True)

    if compiler == "MSVC":
        if "/arch:AVX2" in cxx_flags and "/O2" in cxx_flags:
            report("MSVC Release Flags", "AVX2 & Rapid Inlining detected", ok=True)
        else:
            report("MSVC Release Flags", f"Sub-optimal: {cxx_flags or '(empty)'}", ok=False)
    elif compiler in ("GNU", "Clang", "AppleClang"):
        if "-march=native" in cxx_flags or "-O3" in cxx_flags:
            report("GNU/Clang Release Flags", "Architecture tuning (-march) detected", ok=True)
        else:
            report("GNU/Clang Release Flags", f"Sub-optimal: {cxx_flags or '(empty)'}", ok=False)

    print(f"\n{Colors.BOLD}--- Hardware Acceleration Linkage ---{Colors.RESET}")
    report("OpenMP Linkage",     "Enabled in CMake" if openmp else "Disabled / Failed to link", ok=openmp)
    report("MPI Multi-Node",     "Enabled in CMake" if mpi    else "Disabled (single-node mode)", ok=True)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main():
    print_header()

    print(f"\n{Colors.BOLD}--- Thread Saturation Analysis ---{Colors.RESET}")
    check_openmp()

    scan_cmake_cache()

    print(f"\n{Colors.BOLD}{Colors.GREEN}=================================================={Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.GREEN}   Health Check Complete. Review output above.    {Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.GREEN}=================================================={Colors.RESET}\n")


if __name__ == "__main__":
    main()
