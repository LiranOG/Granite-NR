"""
Shared runner utilities for telemetry CLI orchestration.
"""
import csv
import json
import sys
from pathlib import Path
from typing import Iterable, List, Optional

from granite_analysis.core.models import CflEvent, NanEvent, SimulationStep
from granite_analysis.core.parsers import parse_telemetry_line
from granite_analysis.utils.ui import GraniteUI


def run_telemetry_loop(
    input_stream: Iterable[str],
    ui: GraniteUI,
    is_quiet: bool,
    json_path: Optional[Path],
    csv_path: Optional[Path],
) -> None:
    steps: List[SimulationStep] = []

    try:
        for line in input_stream:
            event = parse_telemetry_line(line)
            if event is None:
                continue

            if isinstance(event, SimulationStep):
                steps.append(event)
                if not is_quiet:
                    ui.render_step(event)
            elif isinstance(event, NanEvent):
                if not is_quiet:
                    ui.render_nan(event)
            elif isinstance(event, CflEvent):
                if not is_quiet:
                    ui.render_cfl(event)
    except KeyboardInterrupt:
        if not is_quiet:
            ui.console.print("\n[bold yellow]Stream interrupted by user.[/bold yellow]")

    if json_path:
        data = [
            {
                "step": s.step,
                "t": s.t,
                "alpha_center": s.alpha_center,
                "ham_l2": s.ham_l2,
                "blocks": s.blocks,
            }
            for s in steps
        ]
        try:
            with json_path.open("w", encoding="utf-8") as f:
                json.dump(data, f, indent=2)
        except FileNotFoundError:
            sys.exit(f"ERROR: --json directory does not exist: {json_path.parent}")

    if csv_path:
        try:
            with csv_path.open("w", encoding="utf-8", newline="") as f:
                writer = csv.writer(f)
                writer.writerow(["step", "t", "alpha_center", "ham_l2", "blocks"])
                for s in steps:
                    writer.writerow([s.step, s.t, s.alpha_center, s.ham_l2, s.blocks])
        except FileNotFoundError:
            sys.exit(f"ERROR: --csv directory does not exist: {csv_path.parent}")
