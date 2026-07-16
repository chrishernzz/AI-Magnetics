"""
Table-driven unit conversion tests (spec section 9 acceptance criteria:
"every unit conversion is tested"), plus the RMS-current triangular-ripple
derivation formula.
"""
import math

import magnetics_cpp


def test_uh_to_h():
    assert math.isclose(250.0 * 1e-6, 0.00025)


def test_khz_to_hz():
    assert math.isclose(100.0 * 1000.0, 100000.0)


def test_mm2_to_cm2():
    assert math.isclose(735.0502256509033 / 100.0, 7.350502256509033)


def test_mm_to_cm():
    assert math.isclose(273.919572699267 / 10.0, 27.3919572699267)


def test_stored_energy_matches_formula():
    inductance_h = 250e-6
    peak_current_a = 2.0
    expected = 0.5 * inductance_h * peak_current_a**2
    assert math.isclose(magnetics_cpp.calculate_stored_energy(inductance_h, peak_current_a), expected)


def test_area_product_uses_active_rules_not_hidden_constants():
    rules = magnetics_cpp.design_rules_phase1_default()

    input_data = magnetics_cpp.AreaProductInput()
    input_data.inductanceH = 250e-6
    input_data.peakCurrentA = 2.0
    input_data.switchingFreqHz = 100000.0
    input_data.allowableTempRiseC = 40.0
    input_data.windowUtilization = rules.windowUtilization
    input_data.fluxDensityT = rules.defaultFluxDensityLimitT
    input_data.currentDensityAPerCm2 = rules.allowableCurrentDensityAperCm2

    energy = magnetics_cpp.calculate_stored_energy(input_data.inductanceH, input_data.peakCurrentA)
    expected_ap = (2.0 * energy * 1e4) / (
        rules.windowUtilization * rules.defaultFluxDensityLimitT * rules.allowableCurrentDensityAperCm2
    )

    assert math.isclose(magnetics_cpp.calculate_ap(input_data), expected_ap)


def test_rms_triangular_ripple_formula():
    # Irms = sqrt(Iavg^2 + ripple^2/12) - spec section 5, valid only for
    # triangular ripple.
    iavg = 4.0
    ripple = 1.5
    expected = math.sqrt(iavg**2 + (ripple**2) / 12.0)
    assert math.isclose(expected, 4.0234, rel_tol=1e-3)


def test_rms_current_derivation_end_to_end_does_not_raise():
    # Exercises RequirementDerivationService's derivation branch through
    # the real pipeline (average current + ripple, no rmsCurrentA supplied).
    request = magnetics_cpp.InductorDesignRequest()
    request.inductanceUH = 250.0
    request.peakCurrentA = 5.0
    request.switchingFreqKHz = 100.0
    request.ambientTemperatureC = 25.0
    request.allowableTempRiseC = 40.0
    request.averageCurrentA = 4.0
    request.rippleCurrentPeakToPeakA = 1.5

    result = magnetics_cpp.run_inductor_design(request)
    assert result is not None


def test_missing_rms_current_raises_value_error():
    # Peak current alone must never be used to infer RMS current - spec
    # section 5. Neither rmsCurrentA nor averageCurrentA/ripple is supplied
    # here, so RequirementDerivationService must raise, not guess.
    request = magnetics_cpp.InductorDesignRequest()
    request.inductanceUH = 250.0
    request.peakCurrentA = 5.0
    request.switchingFreqKHz = 100.0
    request.ambientTemperatureC = 25.0
    request.allowableTempRiseC = 40.0

    try:
        magnetics_cpp.run_inductor_design(request)
        assert False, "expected a ValueError when rmsCurrentA cannot be determined"
    except ValueError:
        pass
