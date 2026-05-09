import pytest
from granite_analysis.core.models import (
    SimulationStep,
    NanEvent,
    CflEvent,
    GridParams,
    VersionInfo,
)
from granite_analysis.core.parsers import parse_telemetry_line


def test_parse_simulation_step():
    line = "step=100 t=10.5 α_center=0.99 ||H||₂=1.2e-4 [Blocks: 5]"
    result = parse_telemetry_line(line)
    assert isinstance(result, SimulationStep)
    assert result.step == 100
    assert result.t == 10.5
    assert result.alpha_center == 0.99
    assert result.ham_l2 == 1.2e-4
    assert result.blocks == 5

def test_parse_simulation_step_nan():
    line = "step=200 t=20.0 α_center=nan ||H||₂=inf"
    result = parse_telemetry_line(line)
    assert isinstance(result, SimulationStep)
    assert result.step == 200
    assert result.t == 20.0
    assert result.alpha_center is None
    assert result.ham_l2 is None
    assert result.blocks is None

def test_parse_nan_event_st():
    line = "[NaN@step=50] ST var=18 (10,10,10) = nan"
    result = parse_telemetry_line(line)
    assert isinstance(result, NanEvent)
    assert result.step == 50
    assert result.var_type == "ST"
    assert result.var_id == 18
    assert result.i == 10
    assert result.j == 10
    assert result.k == 10
    assert result.value == "nan"

def test_parse_nan_event_hy():
    line = "[NaN@step=60] HY var=0 (5,5,5) = inf"
    result = parse_telemetry_line(line)
    assert isinstance(result, NanEvent)
    assert result.step == 60
    assert result.var_type == "HY"
    assert result.var_id == 0

def test_parse_cfl_warn():
    line = "[CFL-WARN] CFL=1.25"
    result = parse_telemetry_line(line)
    assert isinstance(result, CflEvent)
    assert result.cfl_value == 1.25

def test_parse_grid():
    line = "dx=0.125, dt=0.0625"
    result = parse_telemetry_line(line)
    assert isinstance(result, GridParams)
    assert result.dx == 0.125
    assert result.dt == 0.0625

def test_parse_version():
    line = "GRANITE v0.6.7"
    result = parse_telemetry_line(line)
    assert isinstance(result, VersionInfo)
    assert result.version == "0.6.7"

def test_parse_invalid_line():
    line = "This is just some random log output"
    result = parse_telemetry_line(line)
    assert result is None
