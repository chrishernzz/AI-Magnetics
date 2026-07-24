"""
MODE 1: BuckElectricalSolver tests, run through the real pybind11 binding
(magnetics_cpp.solve_buck_topology) rather than reimplementing the formula
in Python - a mismatch here would mean the test and the code share the same
bug, not that the code is correct.
"""
import math

import magnetics_cpp


def _buck_topology_input(**overrides):
    defaults = dict(
        vinMinV=36.0,
        vinMaxV=60.0,
        voutV=12.0,
        ioutA=40.0,
        switchingFreqKHz=500.0,
        rippleCurrentPercent=20.0,
        ambientTemperatureC=25.0,
        allowableTempRiseC=40.0,
    )
    defaults.update(overrides)

    cpp_input = magnetics_cpp.TopologyInput()
    for key, value in defaults.items():
        setattr(cpp_input, key, value)
    return cpp_input


def test_buck_solver_matches_hand_calculation():
    # Golden case, worked by hand:
    # D = Vout/Vin_max = 12/60 = 0.2
    # ripple = Iout * 20% = 8 A
    # L = (Vin-Vout)*D / (fsw*ripple) = 48*0.2 / (500000*8) = 2.4 uH
    # Ipeak = Iout + ripple/2 = 40 + 4 = 44 A
    derived = magnetics_cpp.solve_buck_topology(_buck_topology_input())

    assert math.isclose(derived.inductanceUH, 2.4, rel_tol=1e-9)
    assert math.isclose(derived.peakCurrentA, 44.0, rel_tol=1e-9)
    assert math.isclose(derived.averageCurrentA, 40.0, rel_tol=1e-9)
    assert math.isclose(derived.rippleCurrentPeakToPeakA, 8.0, rel_tol=1e-9)
    assert math.isclose(derived.switchingFreqKHz, 500.0, rel_tol=1e-9)
    # rmsCurrentA is deliberately left for RequirementDerivationService to
    # derive downstream - see BuckElectricalSolver.h.
    assert derived.rmsCurrentA is None


def test_buck_solver_output_feeds_the_existing_pipeline_unchanged():
    # Exercises the actual Mode 1 -> Mode 2 handoff: the derived request is
    # passed straight into run_inductor_design with no modification, the
    # same call Mode 2's direct-entry form makes.
    derived = magnetics_cpp.solve_buck_topology(_buck_topology_input())
    result = magnetics_cpp.run_inductor_design(derived)
    assert result is not None
    assert result.status in ("ok", "no_feasible_design")


def test_buck_solver_rejects_vout_greater_than_vin_max():
    # A buck converter cannot regulate Vout >= Vin - must raise, not
    # silently return a negative/nonsensical inductance.
    bad_input = _buck_topology_input(voutV=72.0)  # > vinMaxV=60
    try:
        magnetics_cpp.solve_buck_topology(bad_input)
        assert False, "expected a ValueError when voutV >= vinMaxV"
    except ValueError:
        pass


def test_buck_solver_rejects_non_positive_ripple_percent():
    bad_input = _buck_topology_input(rippleCurrentPercent=0.0)
    try:
        magnetics_cpp.solve_buck_topology(bad_input)
        assert False, "expected a ValueError for a zero ripple target"
    except ValueError:
        pass


def test_buck_solver_rejects_vout_at_or_above_vin_min():
    # Vout >= Vin_min is rejected even though Vout < Vin_max - the real
    # binding constraint is the low end of the input range (duty cycle is
    # maximum there), a case the old vinMaxV-only check would have wrongly
    # accepted.
    bad_input = _buck_topology_input(vinMinV=24.0, vinMaxV=60.0, voutV=30.0)
    try:
        magnetics_cpp.solve_buck_topology(bad_input)
        assert False, "expected a ValueError when voutV >= vinMinV"
    except ValueError:
        pass


def test_buck_solver_rejects_nan_input():
    bad_input = _buck_topology_input(ioutA=float("nan"))
    try:
        magnetics_cpp.solve_buck_topology(bad_input)
        assert False, "expected a ValueError for a NaN input"
    except ValueError:
        pass


def test_buck_solver_rejects_infinite_input():
    bad_input = _buck_topology_input(switchingFreqKHz=float("inf"))
    try:
        magnetics_cpp.solve_buck_topology(bad_input)
        assert False, "expected a ValueError for an infinite input"
    except ValueError:
        pass
