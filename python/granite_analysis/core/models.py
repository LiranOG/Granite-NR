"""
Strictly typed immutable data models for GRANITE telemetry.
"""
from dataclasses import dataclass
from typing import Optional


@dataclass(frozen=True)
class GridParams:
    dx: float
    dt: float


@dataclass(frozen=True)
class NCellsInfo:
    nx: int
    ny: int
    nz: int


@dataclass(frozen=True)
class SimulationStep:
    step: int
    t: float
    alpha_center: Optional[float]
    ham_l2: Optional[float]
    blocks: Optional[int] = None


@dataclass(frozen=True)
class NanEvent:
    step: int
    var_type: str
    var_id: int
    i: int
    j: int
    k: int
    value: str


@dataclass(frozen=True)
class CflEvent:
    cfl_value: float
    step: Optional[int] = None


@dataclass(frozen=True)
class CheckpointEvent:
    t: float


@dataclass(frozen=True)
class DiagnosticEvent:
    system: str
    status: str


@dataclass(frozen=True)
class PunctureData:
    puncture_id: int
    x: float
    y: float
    z: float


@dataclass(frozen=True)
class VersionInfo:
    version: str


@dataclass(frozen=True)
class IdTypeInfo:
    id_type: str


@dataclass(frozen=True)
class TFinalInfo:
    t_final: float
