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


def _solve(**overrides):
    rules = magnetics_cpp.design_rules_phase1_default()
    return magnetics_cpp.solve_buck_topology(_buck_topology_input(**overrides), rules)


def test_buck_solver_matches_hand_calculation():
    # Golden case, worked by hand:
    # D = Vout/Vin_max = 12/60 = 0.2
    # ripple = Iout * 20% = 8 A
    # L = (Vin-Vout)*D / (fsw*ripple) = 48*0.2 / (500000*8) = 2.4 uH
    # Ipeak = Iout + ripple/2 = 40 + 4 = 44 A
    result = _solve()

    assert math.isclose(result.request.inductanceUH, 2.4, rel_tol=1e-9)
    assert math.isclose(result.request.peakCurrentA, 44.0, rel_tol=1e-9)
    assert math.isclose(result.request.averageCurrentA, 40.0, rel_tol=1e-9)
    assert math.isclose(result.request.rippleCurrentPeakToPeakA, 8.0, rel_tol=1e-9)
    assert math.isclose(result.request.switchingFreqKHz, 500.0, rel_tol=1e-9)
    # rmsCurrentA is deliberately left for RequirementDerivationService to
    # derive downstream - see BuckElectricalSolver.h.
    assert result.request.rmsCurrentA is None

    # Both operating points are now real, not just Vin_max.
    assert math.isclose(result.atVinMax.vinV, 60.0, rel_tol=1e-9)
    assert math.isclose(result.atVinMax.dutyCycle, 0.2, rel_tol=1e-9)
    assert math.isclose(result.atVinMax.peakCurrentA, 44.0, rel_tol=1e-9)
    assert math.isclose(result.atVinMin.vinV, 36.0, rel_tol=1e-9)
    # D = 12/36 = 1/3 at Vin_min - duty cycle is worst (highest) at Vin_min.
    assert math.isclose(result.atVinMin.dutyCycle, 12.0 / 36.0, rel_tol=1e-9)
    assert result.worstCaseDutyCycleAt == "VinMin"
    # Ripple/peak current are worst at Vin_max (ripple grows with Vin - Vout).
    assert result.worstCaseRippleAt == "VinMax"
    assert result.worstCasePeakAt == "VinMax"

    assert result.conductionMode == magnetics_cpp.ConductionMode.CCM
    assert len(result.assumptions) == 4
    assert len(result.warnings) == 0


def test_buck_solver_output_feeds_the_existing_pipeline_unchanged():
    # Exercises the actual Mode 1 -> Mode 2 handoff: the derived request is
    # passed straight into run_inductor_design with no modification, the
    # same call Mode 2's direct-entry form makes.
    result = _solve()
    pipeline_result = magnetics_cpp.run_inductor_design(result.request)
    assert pipeline_result is not None
    assert pipeline_result.status in ("ok", "no_feasible_design")


def test_buck_solver_rejects_vout_greater_than_vin_max():
    # A buck converter cannot regulate Vout >= Vin - must raise, not
    # silently return a negative/nonsensical inductance.
    try:
        _solve(voutV=72.0)  # > vinMaxV=60
        assert False, "expected a ValueError when voutV >= vinMaxV"
    except ValueError:
        pass


def test_buck_solver_rejects_non_positive_ripple_percent():
    try:
        _solve(rippleCurrentPercent=0.0)
        assert False, "expected a ValueError for a zero ripple target"
    except ValueError:
        pass


def test_buck_solver_rejects_vout_at_or_above_vin_min():
    # Vout >= Vin_min is rejected even though Vout < Vin_max - the real
    # binding constraint is the low end of the input range (duty cycle is
    # maximum there), a case the old vinMaxV-only check would have wrongly
    # accepted.
    try:
        _solve(vinMinV=24.0, vinMaxV=60.0, voutV=30.0)
        assert False, "expected a ValueError when voutV >= vinMinV"
    except ValueError:
        pass


def test_buck_solver_rejects_nan_input():
    try:
        _solve(ioutA=float("nan"))
        assert False, "expected a ValueError for a NaN input"
    except ValueError:
        pass


def test_buck_solver_rejects_infinite_input():
    try:
        _solve(switchingFreqKHz=float("inf"))
        assert False, "expected a ValueError for an infinite input"
    except ValueError:
        pass


def test_buck_solver_rejects_ripple_above_maximum():
    rules = magnetics_cpp.design_rules_phase1_default()
    bad_input = _buck_topology_input(rippleCurrentPercent=rules.maximumRippleCurrentPercent + 1.0)
    try:
        magnetics_cpp.solve_buck_topology(bad_input, rules)
        assert False, "expected a ValueError for ripple above the Phase 1 input-sanity bound"
    except ValueError:
        pass


def test_buck_solver_ccm_boundary_warns_but_does_not_reject():
    # A duty cycle / ripple combination that pushes the minimum inductor
    # current to (near) zero at Vin_max should classify as CCMBoundary with
    # a warning, not raise - only a genuinely negative minimum current (DCM)
    # is rejected.
    #
    # At Vin_max=60, Vout=12: ripple = Iout * ripple%. Minimum inductor
    # current = Iout - ripple/2 = Iout * (1 - ripple%/200). This hits zero
    # exactly at ripple% = 200%, which exceeds the default 100% input-sanity
    # cap - so this case is exercised with a relaxed rules object instead of
    # relying on the default cap.
    rules = magnetics_cpp.design_rules_phase1_default()
    rules.maximumRippleCurrentPercent = 250.0
    result = magnetics_cpp.solve_buck_topology(_buck_topology_input(rippleCurrentPercent=200.0), rules)
    assert result.conductionMode == magnetics_cpp.ConductionMode.CCMBoundary
    assert len(result.warnings) >= 1


def test_buck_solver_rejects_dcm_operating_point():
    # Ripple large enough that minimum inductor current goes negative at
    # Vin_max - a real DCM operating point Phase 1's CCM-only formulas do
    # not support.
    rules = magnetics_cpp.design_rules_phase1_default()
    rules.maximumRippleCurrentPercent = 400.0
    try:
        magnetics_cpp.solve_buck_topology(_buck_topology_input(rippleCurrentPercent=300.0), rules)
        assert False, "expected a ValueError for a DCM operating point"
    except ValueError:
        pass
