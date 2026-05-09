#!/usr/bin/env python3
"""
GRANITE Universal Simulation Tracker
Lightweight CLI Orchestrator.
"""
import argparse
import sys
from pathlib import Path

from granite_analysis.core.runners import run_telemetry_loop
from granite_analysis.utils.ui import GraniteUI


def main() -> None:
    parser = argparse.ArgumentParser(description="GRANITE Live Simulation Tracker")
    parser.add_argument(
        "logfile", nargs="?", type=Path, help="Path to telemetry log file. If omitted, reads from stdin."
    )
    parser.add_argument("--quiet", action="store_true", help="Suppress terminal output")
    parser.add_argument("--json", type=Path, help="Export captured telemetry to JSON file")
    parser.add_argument("--csv", type=Path, help="Export captured telemetry to CSV file")

    args = parser.parse_args()
    ui = GraniteUI()

    if args.logfile:
        try:
            with args.logfile.open("r", encoding="utf-8") as f:
                run_telemetry_loop(f, ui, args.quiet, args.json, args.csv)
        except FileNotFoundError:
            sys.exit(f"ERROR: log file not found: {args.logfile}")
    else:
        run_telemetry_loop(sys.stdin, ui, args.quiet, args.json, args.csv)


if __name__ == "__main__":
    main()
