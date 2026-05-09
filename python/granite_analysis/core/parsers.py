"""
Stateless parsing logic for GRANITE telemetry output.
"""
import math
import re
from pathlib import Path
from typing import Iterator, Optional, Union

from granite_analysis.core.models import (
    CflEvent,
    CheckpointEvent,
    DiagnosticEvent,
    GridParams,
    IdTypeInfo,
    NanEvent,
    NCellsInfo,
    PunctureData,
    SimulationStep,
    TFinalInfo,
    VersionInfo,
)

# ── Compiled regex patterns ───────────────────────────────────────────────────
_RE_STEP = re.compile(
    r"step=(\d+)\s+t=([\d.eE+\-]+)\s+"
    r"α_center=([\d.eE+\-naninf]+)\s+"
    r"(?:‖|\|+)H(?:‖|\|+)[₂2]=([\d.eE+\-naninf]+)"
    r"(?:.*?\[Blocks:\s*(\d+)\])?"
)
_RE_TFINAL = re.compile(r"t_final\s*=\s*([\d.eE+\-]+)")
_RE_GRID = re.compile(r"dx\s*=\s*([\d.eE+\-]+),?\s*dt\s*=\s*([\d.eE+\-]+)")
_RE_NCELLS = re.compile(r"(?:AMR.*?|Grid:)\s*(\d+)\s*[x×]\s*(\d+)\s*[x×]\s*(\d+)")
_RE_NAN_ST = re.compile(
    r"\[NaN@step=(\d+)\]\s+ST\s+var=(\d+)\s+\((\d+),(\d+),(\d+)\)\s*=\s*(\S+)"
)
_RE_NAN_HY = re.compile(
    r"\[NaN@step=(\d+)\]\s+HY\s+var=(\d+)\s+\((\d+),(\d+),(\d+)\)\s*=\s*(\S+)"
)
_RE_CFL_W = re.compile(r"\[CFL-(?:WARN|GUARD)\].*CFL=([\d.eE+\-]+)")
_RE_GRANITE = re.compile(r"GRANITE\s+v([\d.]+)")
_RE_ID_TYPE = re.compile(
    r"(Two-Punctures|Brill-Lindquist|Bowen-York|Gauge wave|Flat Minkowski)", re.I
)
_RE_CHKPT = re.compile(r"(?:Writing checkpoint|Checkpoint).*?t=([\d.eE+\-]+)")
_RE_DIAG_ST = re.compile(r"\[NaN-DIAG\]\s+ST_RHS:\s*(.*)")
_RE_DIAG_HY = re.compile(r"\[NaN-DIAG\]\s+HY_RHS:\s*(.*)")
_RE_PUNCH = re.compile(
    r"Puncture[_\s]+(\d+).*?x=([\d.\-]+).*?y=([\d.\-]+).*?z=([\d.\-]+)", re.I
)

TelemetryEvent = Union[
    SimulationStep,
    NanEvent,
    CflEvent,
    CheckpointEvent,
    DiagnosticEvent,
    PunctureData,
    GridParams,
    NCellsInfo,
    VersionInfo,
    IdTypeInfo,
    TFinalInfo,
]


def _safe_float(s: str) -> Optional[float]:
    """Safely convert a string to a finite float, returning None otherwise."""
    try:
        v = float(s)
        return v if math.isfinite(v) else None
    except ValueError:
        return None


def parse_telemetry_line(line: str) -> Optional[TelemetryEvent]:
    """Parse a single line of telemetry output into a typed data model."""
    line = line.rstrip("\n")
    if not line:
        return None

    if m := _RE_STEP.search(line):
        blocks = int(m.group(5)) if m.group(5) else None
        return SimulationStep(
            step=int(m.group(1)),
            t=float(m.group(2)),
            alpha_center=_safe_float(m.group(3)),
            ham_l2=_safe_float(m.group(4)),
            blocks=blocks,
        )

    if m := _RE_NAN_ST.search(line):
        return NanEvent(
            step=int(m.group(1)),
            var_type="ST",
            var_id=int(m.group(2)),
            i=int(m.group(3)),
            j=int(m.group(4)),
            k=int(m.group(5)),
            value=m.group(6),
        )

    if m := _RE_NAN_HY.search(line):
        return NanEvent(
            step=int(m.group(1)),
            var_type="HY",
            var_id=int(m.group(2)),
            i=int(m.group(3)),
            j=int(m.group(4)),
            k=int(m.group(5)),
            value=m.group(6),
        )

    if m := _RE_CFL_W.search(line):
        return CflEvent(
            cfl_value=float(m.group(1)),
        )

    if m := _RE_GRID.search(line):
        return GridParams(
            dx=float(m.group(1)),
            dt=float(m.group(2)),
        )

    if m := _RE_NCELLS.search(line):
        return NCellsInfo(
            nx=int(m.group(1)),
            ny=int(m.group(2)),
            nz=int(m.group(3)),
        )

    if m := _RE_TFINAL.search(line):
        return TFinalInfo(t_final=float(m.group(1)))

    if m := _RE_GRANITE.search(line):
        return VersionInfo(version=m.group(1))

    if m := _RE_ID_TYPE.search(line):
        return IdTypeInfo(id_type=m.group(1))

    if m := _RE_CHKPT.search(line):
        return CheckpointEvent(t=float(m.group(1)))

    if m := _RE_DIAG_ST.search(line):
        return DiagnosticEvent(system="ST_RHS", status=m.group(1).strip())

    if m := _RE_DIAG_HY.search(line):
        return DiagnosticEvent(system="HY_RHS", status=m.group(1).strip())

    if m := _RE_PUNCH.search(line):
        return PunctureData(
            puncture_id=int(m.group(1)),
            x=float(m.group(2)),
            y=float(m.group(3)),
            z=float(m.group(4)),
        )

    return None


def parse_telemetry_file(file_path: Path) -> Iterator[TelemetryEvent]:
    """Lazily parse a telemetry log file."""
    with file_path.open("r", encoding="utf-8") as f:
        for line in f:
            if event := parse_telemetry_line(line):
                yield event
