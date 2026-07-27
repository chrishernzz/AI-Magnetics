"""
Golden reference test suite (spec section 12): 12 cases exercising the qualitative
outcomes the "End of Phase 1" spec asks for, each run through the real engine
(magnetics_cpp) against the actual loaded material/core/core-loss-coefficient
databases - never a fabricated "reference source" number. Where a case needed a
specific real input to reach a specific real code path (e.g. which core triggers
a small-gap warning), that input was found by probing the live engine directly,
not guessed or invented - see each test's docstring for what was probed and why.

These cases are labeled "independently calculated" (by this test file, against
the real engine, at test-collection time or by direct hand formula) - never
"manufacturer application note," since no such sourced example numbers exist in
this repository to cite.
"""
import math

import magnetics_cpp


def _design_request(**overrides):
    defaults = dict(
        inductanceUH=100.0,
        peakCurrentA=2.0,
        rmsCurrentA=1.5,
        switchingFreqKHz=100.0,
        ambientTemperatureC=25.0,
        allowableTempRiseC=60.0,
    )
    defaults.update(overrides)
    req = magnetics_cpp.InductorDesignRequest()
    for key, value in defaults.items():
        if value is not None:
            setattr(req, key, value)
    return req


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


def test_case1_normal_dual_line_buck_solves_and_hands_off_cleanly():
    """Case 1: Vin 36-60V / Vout 12V / 40A / 500kHz / 20% ripple - hand-worked:
    D = Vout/Vin_max = 12/60 = 0.2; ripple = Iout*20% = 8A;
    L = (Vin-Vout)*D / (fsw*ripple) = 48*0.2 / (500000*8) = 2.4 uH; Ipeak = 40+4 = 44A.
    Feeds straight into run_inductor_design and must produce a real, honest status.
    No candidate reaches Phase1Recommended - confirmed structurally unreachable in
    Phase 1 (RecommendationStatus.h: ThermalValidation always sets
    isPreliminaryEstimate=true), so the qualitative claim here is "solves, and
    every passing candidate is honestly capped at PreliminaryCandidate," not
    "reaches the top tier" (which no real Phase 1 input ever does)."""
    rules = magnetics_cpp.design_rules_phase1_default()
    derived = magnetics_cpp.solve_buck_topology(_buck_topology_input(), rules)

    assert math.isclose(derived.request.inductanceUH, 2.4, rel_tol=1e-9)
    assert math.isclose(derived.request.peakCurrentA, 44.0, rel_tol=1e-9)
    assert math.isclose(derived.request.rippleCurrentPeakToPeakA, 8.0, rel_tol=1e-9)
    assert derived.conductionMode == magnetics_cpp.ConductionMode.CCM

    result = magnetics_cpp.run_inductor_design(derived.request)
    assert result.status in ("ok", "no_feasible_design")
    if result.status == "ok":
        tiers = {c.recommendation.tier for c in result.candidates}
        assert magnetics_cpp.RecommendationTier.Phase1Recommended not in tiers
        assert all(t == magnetics_cpp.RecommendationTier.PreliminaryCandidate for t in tiers)


def test_case2_high_ripple_90_percent_solves_under_the_phase1_cap():
    """Case 2: 90% ripple is under the Phase 1 default maximumRippleCurrentPercent
    (100%) input-sanity bound, so it must solve, not raise - and the physics must
    move the right direction: more ripple current, less required inductance, same
    conduction mode, versus the 20%-ripple case 1 baseline."""
    rules = magnetics_cpp.design_rules_phase1_default()
    baseline = magnetics_cpp.solve_buck_topology(_buck_topology_input(rippleCurrentPercent=20.0), rules)
    high = magnetics_cpp.solve_buck_topology(_buck_topology_input(rippleCurrentPercent=90.0), rules)

    assert high.request.rippleCurrentPeakToPeakA > baseline.request.rippleCurrentPeakToPeakA
    assert high.request.inductanceUH < baseline.request.inductanceUH
    assert high.conductionMode == magnetics_cpp.ConductionMode.CCM


def test_case3_ccm_boundary_warns_but_does_not_reject():
    """Case 3: a ripple percent that pushes minimum inductor current to exactly
    zero at Vin_max classifies as CCMBoundary with a warning, not a rejection -
    only a genuinely negative minimum current (DCM) is rejected. Exercised with a
    relaxed rules object since the exact boundary (200% ripple) exceeds the
    default 100% input-sanity cap."""
    rules = magnetics_cpp.design_rules_phase1_default()
    rules.maximumRippleCurrentPercent = 250.0
    result = magnetics_cpp.solve_buck_topology(_buck_topology_input(rippleCurrentPercent=200.0), rules)
    assert result.conductionMode == magnetics_cpp.ConductionMode.CCMBoundary
    assert len(result.warnings) >= 1


def test_case4_dcm_operating_point_is_rejected():
    """Case 4: ripple large enough that minimum inductor current goes negative at
    Vin_max is a real DCM operating point Phase 1's CCM-only formulas do not
    support - must raise, never silently return a nonsensical result."""
    rules = magnetics_cpp.design_rules_phase1_default()
    rules.maximumRippleCurrentPercent = 400.0
    try:
        magnetics_cpp.solve_buck_topology(_buck_topology_input(rippleCurrentPercent=300.0), rules)
        assert False, "expected a ValueError for a DCM operating point"
    except ValueError:
        pass


def test_case5_vout_at_or_above_vin_min_is_rejected():
    """Case 5: Vout >= Vin_min is rejected even though Vout < Vin_max - the real
    binding constraint is the low end of the input range (duty cycle is maximum
    there), the specific case the old vinMaxV-only check would have wrongly
    accepted (see BuckElectricalSolver.cpp's Vout-vs-VinMin fix)."""
    rules = magnetics_cpp.design_rules_phase1_default()
    try:
        magnetics_cpp.solve_buck_topology(_buck_topology_input(vinMinV=24.0, vinMaxV=60.0, voutV=30.0), rules)
        assert False, "expected a ValueError when voutV >= vinMinV"
    except ValueError as e:
        assert "vinMinV" in str(e)


def test_case6_saturation_failure_rejects_every_candidate():
    """Case 6: a target inductance/current combination that saturates every
    real candidate core - found by probing the live engine (470uH, 40A peak,
    28A rms, 80kHz against the real database rejects all 3 area-product-feasible
    cores on SaturationValidation). Re-probed after the CoreShape database fix
    (60->168 cores) added real MPP-toroid coverage - the original 12A-peak
    trigger now has a real passing candidate (C055439A2X2, the exact MPP-60
    toroid this fix was built to surface), so it no longer demonstrates a
    saturation rejection; 40A peak still genuinely rejects every candidate."""
    result = magnetics_cpp.run_inductor_design(
        _design_request(inductanceUH=470.0, peakCurrentA=40.0, rmsCurrentA=28.0, switchingFreqKHz=80.0)
    )
    assert result.status == "no_feasible_design"
    assert len(result.rejectedCandidates) > 0
    assert all(
        any(r.checkName == "SaturationValidation" for r in c.rejectionReasons) for c in result.rejectedCandidates
    )


def test_case7_gap_tolerance_sweep_fails_even_when_nominal_passes():
    """Case 7: a tight requested inductance tolerance combined with the default
    +-10% mechanical gap tolerance can push the gap's mechanical extremes outside
    tolerance even when the as-designed (nominal) gap is dead on target - the
    exact real E100/60/28-3C90 catalog-geometry scenario already verified at the
    engine level in tests/cpp/GapToleranceTests.cpp's
    testGapToleranceCanFailEvenWhenNominalPasses, reproduced here through the
    Python binding to confirm it holds end-to-end, not just in the C++ unit test."""
    core = magnetics_cpp.CoreCandidate()
    core.partNumber = "E100/60/28-3C90"
    core.material = "3C90"
    core.mu = 2249.28
    core.al = 7584.855918773515
    core.aeMm2 = 735.0502256509033
    core.waMm2 = 2138.7025
    core.leMm = 273.919572699267
    core.mltMm = 110.0
    core.areaProductCm4 = 0.0
    core.meetsAreaProduct = True

    rules = magnetics_cpp.design_rules_phase1_default()
    result = magnetics_cpp.design_turns_and_gap(core, 2.0, 0.5, rules)

    assert result.converged
    assert result.withinTolerance  # the nominal design itself is precise
    assert not result.inductanceWithinToleranceAcrossGapRange  # but the realistic gap-tolerance sweep fails it


def test_case8_winding_fit_fails_on_physical_model_despite_passing_raw_copper_fill():
    """Case 8: found by probing the live engine for a real candidate whose raw
    copper-only fillFactor is within rules.maximumFillFactor (0.6) but whose
    physicalWindowFillFactor (insulation, packing factor, bobbin/margin/lead-exit
    derates - see WindingDesign.h) exceeds it - the direct demonstration of the
    §1.7 behavior change from Commit 8. 10uH / 3.9A peak / 3A rms / 100kHz
    against the real database routes to core 00A1808E026 with exactly this split."""
    result = magnetics_cpp.run_inductor_design(
        _design_request(inductanceUH=10.0, peakCurrentA=3.9, rmsCurrentA=3.0, switchingFreqKHz=100.0)
    )
    matches = [c for c in result.candidates + result.rejectedCandidates if c.core.partNumber == "00A1808E026"]
    assert matches, "expected core 00A1808E026 to be a candidate for this request"
    candidate = matches[0]

    assert candidate.winding.fitsWindow  # raw copper fill alone would have passed
    assert not candidate.winding.fitsPhysicalWindow  # the realistic physical model fails it
    assert not candidate.passed
    assert any(r.checkName == "WindingFitValidation" for r in candidate.rejectionReasons)


def test_case9_small_gap_manufacturability_warning_caps_at_preliminary_not_rejection():
    """Case 9: found by probing the live engine - 100uH/2A peak/1.5A rms/100kHz
    against the real database produces several real candidates (e.g.
    B65646A0000R027, gap 0.02mm) whose calculated gap is below
    rules.minManufacturableGapMm - a real, honest warning, not a fabricated one,
    and the design still passes (small gap is a manufacturability caveat, not a
    physical failure) but caps at PreliminaryCandidate rather than the top tier."""
    result = magnetics_cpp.run_inductor_design(_design_request())
    assert result.status == "ok"
    small_gap_candidates = [c for c in result.candidates if c.turnsAndGap.smallGapWarning]
    assert small_gap_candidates, "expected at least one real candidate with a small-gap warning"
    for c in small_gap_candidates:
        assert c.passed
        assert c.turnsAndGap.smallGapWarningReason
        assert c.recommendation.tier != magnetics_cpp.RecommendationTier.Phase1Recommended


def test_case10_missing_core_loss_data_is_never_silently_treated_as_zero():
    """Case 10: no rippleCurrentPeakToPeakA supplied (the default, common case) -
    core loss must be honestly not_evaluated for every candidate, never silently
    treated as zero, and lossSummary must name the gap rather than presenting a
    copper-only figure as if it were complete. Note: core-loss coverage is not
    itself one of the six named ValidationResult checks classifyRecommendation()
    reads (RecommendationStatus.h), so checksNotEvaluatedCount does not count it -
    the tier still never reaches Phase1Recommended here, but for the separate,
    always-true reason that ThermalValidation is always a preliminary estimate
    (see test_case11 below), not because of the missing core loss specifically."""
    result = magnetics_cpp.run_inductor_design(_design_request())
    assert result.status == "ok"
    assert all(c.losses.coreLossStatus == magnetics_cpp.EvaluationStatus.NotEvaluated for c in result.candidates)
    assert all(not c.lossSummary.isCompleteTotal for c in result.candidates)
    assert all("partial coverage" in c.lossSummary.label for c in result.candidates)
    assert all(c.recommendation.tier != magnetics_cpp.RecommendationTier.Phase1Recommended for c in result.candidates)


def test_case11_thermal_stays_preliminary_even_with_full_loss_coverage():
    """Case 11: found by probing the live engine - supplying a real ripple
    current (0.5A) alongside the case 9/10 baseline gives at least one real
    candidate (E100/60/28-3C90) both copper AND core loss Evaluated - full known
    loss coverage - yet thermal.status is still only ever PreliminaryThermalEstimate
    (never a "fully evaluated" thermal value exists in Phase 1 - see
    ThermalEvaluation.h), so this candidate still caps at PreliminaryCandidate
    despite having the most complete loss data possible in this engine today."""
    result = magnetics_cpp.run_inductor_design(_design_request(rippleCurrentPeakToPeakA=0.5))
    assert result.status == "ok"
    full_coverage = [
        c
        for c in result.candidates
        if c.losses.copperLossStatus == magnetics_cpp.EvaluationStatus.Evaluated
        and c.losses.coreLossStatus == magnetics_cpp.EvaluationStatus.Evaluated
    ]
    assert full_coverage, "expected at least one real candidate with full copper+core loss coverage"
    for c in full_coverage:
        assert c.thermal.status == magnetics_cpp.ThermalStatus.PreliminaryThermalEstimate
        assert c.recommendation.tier == magnetics_cpp.RecommendationTier.PreliminaryCandidate


def test_case12_one_turn_high_current_design_exercises_physical_description_and_current_sharing():
    """Case 12: found by probing the live engine - a very small target inductance
    (0.05uH) at high current (20A peak/15A rms/200kHz) against the real database
    routes to a real core (B65807J0000R041) at exactly turns=1 with 5 parallel
    strands, exercising both the one-turn physicalDescription path and the
    current-sharing derate applied for parallel-strand bundles."""
    result = magnetics_cpp.run_inductor_design(
        _design_request(inductanceUH=0.05, peakCurrentA=20.0, rmsCurrentA=15.0, switchingFreqKHz=200.0)
    )
    matches = [
        c for c in result.candidates + result.rejectedCandidates if c.turnsAndGap.turns == 1 and c.turnsAndGap.converged
    ]
    assert matches, "expected at least one real one-turn candidate"
    candidate = matches[0]

    assert candidate.winding.parallelStrands > 1
    assert candidate.winding.physicalDescription
    assert "single strand" not in candidate.winding.physicalDescription
    assert math.isclose(
        candidate.winding.effectiveCurrentDensityAperMm2,
        candidate.winding.currentDensityAperMm2 / magnetics_cpp.design_rules_phase1_default().currentSharingDerateFactor,
        rel_tol=1e-9,
    )


def test_case13_mpp_toroid_c055439a2x2_reachable_after_database_fix():
    """Case 13: regression test for a real bug report. A coworker tested a
    reference design using Magnetics C055439A2 (MPP-60 toroid) and it was
    completely absent from the tool. Root cause, confirmed by directly
    querying the live PyOpenMagnetics database: the export script's
    per-vendor cap (15) was entirely consumed by Kool Mu/Edge/XFlux toroids
    - a different powder family - before MPP was ever reached, and the
    exact reference (C055439A2X2, the epoxy-coated variant PyOpenMagnetics
    actually carries) was the 35th of 50 real physical sizes under the
    "MPP 60" material name, past the old per-material cap of 4.

    Fixed in scripts/export_real_data.py: the vendor cap is now tracked per
    (vendor, shape) so one powder family can't crowd out another, and
    C055439A2X2 is explicitly prioritized so it wins its slot in the
    per-material cap instead of losing to an arbitrary same-name sibling
    from PyOpenMagnetics' iteration order (see PRIORITY_CORE_REFERENCES).

    This does not replay the coworker's exact application point - their
    reference sheet gives no switching frequency, so one can't be assumed
    without fabricating an input. Instead it uses a request under which
    this exact part is independently verified reachable end-to-end through
    the live pipeline (found by probing, same as every other case here).

    Known open discrepancy, not resolved by this test: the reference
    sheet lists AL = 135 nH/turn^2 for this core; the real geometry
    PyOpenMagnetics provides (Ae=449.49mm^2, Le=106.94mm, mu=60) computes
    to AL = 316.9 nH/turn^2 via the standard mu0*mu_r*Ae/Le formula - a
    ~2.3x difference. Attempts to fetch the Magnetics datasheet directly
    to resolve it were blocked (HTTP 403). Flagged, not silently trusted
    either way."""
    result = magnetics_cpp.run_inductor_design(
        _design_request(inductanceUH=470.0, peakCurrentA=12.0, rmsCurrentA=8.0, switchingFreqKHz=80.0)
    )
    assert result.status == "ok"
    matches = [c for c in result.candidates if c.core.partNumber == "C055439A2X2"]
    assert matches, "expected the real MPP-60 toroid C055439A2X2 to be a reachable candidate"
    candidate = matches[0]
    assert candidate.core.material == "MPP 60"
    assert candidate.core.coreShape == "Toroid"
    assert candidate.core.shapeFamily == "T"
    assert candidate.core.vendor == "Magnetics"
