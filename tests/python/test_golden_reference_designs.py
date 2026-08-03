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

import pytest

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
    No candidate reaches PASS - confirmed structurally unreachable in
    Phase 1 (RecommendationStatus.h: ThermalValidation always sets
    isPreliminaryEstimate=true), so the qualitative claim here is "solves, and
    every passing candidate is honestly capped at CONDITIONAL_PASS," not
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
        assert magnetics_cpp.RecommendationTier.Pass not in tiers
        assert all(t == magnetics_cpp.RecommendationTier.ConditionalPass for t in tiers)


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


#Case 6 removed (was test_case6_flux_aware_seed_finds_real_design_that_used_to_saturate): it verified the
#flux-aware turns/gap seed on a 470uH/40A peak/28A rms/80kHz ferrite scenario, asserting a specific ferrite
#top pick (B65733A0000R027, N27) that correctly beat an inferior, correctly-demoted-but-still-real ferrite
#candidate (E100/60/28-3C90) once ThermalEvaluation stopped using a flat Rth constant. Both of those parts
#were ferrite/machined-gap cores; the database was subsequently narrowed to Magnetics MPP powder toroids and
#powder E-cores only, so neither part - nor any ferrite core at all - exists in real_cores.csv anymore, and
#this scenario can no longer be constructed against the live database. The underlying methodology fix (the
#flux-aware seed itself) is still covered end-to-end for a real database part today: see
#test_case14_mode2_rms_plus_ripple_derives_peak_and_evaluates_saturation below for saturation-aware turns
#seeding on real powder candidates, and tests/cpp/GapToleranceTests.cpp's
#testFluxAwareSeedFindsFeasibleDesignOnSyntheticFerriteCore/testRmsFloorRaisesTurnsSeedWhenPeakAbsent for the
#machined-gap math itself (now exercised via an illustrative/synthetic ferrite fixture instead of a live
#catalog row - there is no real ferrite core left in this project's scope to reproduce the original,
#end-to-end ferrite-vs-ferrite ranking regression this case existed to catch). A live end-to-end
#regression covering the flux-aware seed specifically on a real, currently-cataloged powder core is a real
#coverage gap left by this removal and would need a human to probe the live engine (as every other case in
#this file was built) to find a genuine real-part scenario that exercises it, rather than inventing one here.


def test_case7_gap_tolerance_sweep_fails_even_when_nominal_passes():
    """Case 7: a tight requested inductance tolerance combined with the default
    +-10% mechanical gap tolerance can push the gap's mechanical extremes outside
    tolerance even when the as-designed (nominal) gap is dead on target - the
    same illustrative/synthetic ferrite geometry (originally the real
    E100/60/28-3C90 catalog row, since removed from real_cores.csv now that the
    database is scoped to Magnetics powder cores only) already verified at the
    engine level in tests/cpp/GapToleranceTests.cpp's
    testGapToleranceCanFailEvenWhenNominalPasses, reproduced here through the
    Python binding to confirm it holds end-to-end, not just in the C++ unit test.
    This test hand-builds a CoreCandidate directly rather than looking one up from
    the live database, so it does not depend on any specific catalog row existing.
    peakCurrentA=None and rmsCurrentA=0.0 (the material has no relevant Bmax data
    plugged in here either) means the flux-aware seed never activates - this stays
    a pure inductance/gap-tolerance check, unaffected by that fix or by the later
    RMS-current-as-lower-bound fix."""
    core = magnetics_cpp.CoreCandidate()
    core.partNumber = "SYNTHETIC-FERRITE-3C90"
    core.material = "3C90"
    core.mu = 2249.28
    core.al = 7584.855918773515
    core.aeMm2 = 735.0502256509033
    core.waMm2 = 2138.7025
    core.leMm = 273.919572699267
    core.mltMm = 110.0
    core.areaProductCm4 = 0.0
    core.meetsAreaProduct = True

    material = magnetics_cpp.MaterialCandidate()
    rules = magnetics_cpp.design_rules_phase1_default()
    result = magnetics_cpp.design_turns_and_gap(core, material, 2.0, 0.5, None, 0.0, rules)

    assert result.converged
    assert result.withinTolerance  #the nominal design itself is precise
    assert not result.inductanceWithinToleranceAcrossGapRange  #but the realistic gap-tolerance sweep fails it


def test_case8_winding_fit_fails_on_physical_model_despite_passing_raw_copper_fill():
    """Case 8: found by probing the live engine for a real candidate whose raw
    copper-only fillFactor is within rules.maximumFillFactor (0.6) but whose
    physicalWindowFillFactor (insulation, packing factor, bobbin/margin/lead-exit
    derates - see WindingDesign.h) exceeds it - the direct demonstration of the
    §1.7 behavior change from Commit 8. 10uH / 3.9A peak / 3A rms / 100kHz
    against the real database routes to core 0055130AY (real Magnetics MPP 125
    toroid) with exactly this split. (Originally routed to 00A1808E026, a
    Kool Mu E-core, which the winding-window-area fix in WindingDesign.cpp -
    E-core rows have no real Wa data yet, see data/real_cores.csv's known gap -
    now conservatively fails BOTH the raw and physical checks rather than
    exhibiting the specific raw-passes/physical-fails split this test wants;
    re-probed against a core with real window-area data instead.)"""
    result = magnetics_cpp.run_inductor_design(
        _design_request(inductanceUH=10.0, peakCurrentA=3.9, rmsCurrentA=3.0, switchingFreqKHz=100.0)
    )
    matches = [c for c in result.candidates + result.rejectedCandidates if c.core.partNumber == "0055130AY"]
    assert matches, "expected core 0055130AY to be a candidate for this request"
    candidate = matches[0]

    assert candidate.winding.fitsWindow  #raw copper fill alone would have passed
    assert not candidate.winding.fitsPhysicalWindow  #the realistic physical model fails it
    assert not candidate.passed
    assert any(r.checkName == "WindingFitValidation" for r in candidate.rejectionReasons)


@pytest.mark.xfail(
    reason="smallGapWarning only exists on the machined-gap branch of TurnsAndGapDesign.cpp (a real "
    "discrete gap ground into a ferrite two-piece core - see GapDesign.h); the database was narrowed to "
    "Magnetics MPP/E-core powder parts only, which all use the distributed-gap branch and never populate "
    "gapMm/smallGapWarning at all (see PermeabilityRolloff.h). This scenario (originally probed against "
    "a ferrite candidate, B65646A0000R027, that no longer exists) is not currently constructible against "
    "this database - not an engine bug, a real scope gap. Revisit if ferrite scope is ever re-added.",
    strict=False,
)
def test_case9_small_gap_manufacturability_warning_caps_at_preliminary_not_rejection():
    """Case 9: originally found by probing the live engine - 100uH/2A peak/1.5A rms/100kHz
    against the (then ferrite-inclusive) database produced several real candidates (e.g.
    B65646A0000R027, gap 0.02mm) whose calculated gap is below
    rules.minManufacturableGapMm - a real, honest warning, not a fabricated one,
    and the design still passes (small gap is a manufacturability caveat, not a
    physical failure) but caps at CONDITIONAL_PASS rather than the top tier. See
    the xfail reason above for why this can no longer pass against the current
    MPP/E-core-only powder database."""
    result = magnetics_cpp.run_inductor_design(_design_request())
    assert result.status == "ok"
    small_gap_candidates = [c for c in result.candidates if c.turnsAndGap.smallGapWarning]
    assert small_gap_candidates, "expected at least one real candidate with a small-gap warning"
    for c in small_gap_candidates:
        assert c.passed
        assert c.turnsAndGap.smallGapWarningReason
        assert c.recommendation.tier != magnetics_cpp.RecommendationTier.Pass


def test_case10_missing_core_loss_data_is_never_silently_treated_as_zero():
    """Case 10: no rippleCurrentPeakToPeakA supplied (the default, common case) -
    core loss must be honestly not_evaluated for every candidate, never silently
    treated as zero, and lossSummary must name the gap rather than presenting a
    copper-only figure as if it were complete. Note: core-loss coverage is not
    itself one of the named ValidationResult checks determineRecommendationStatus()
    reads (RecommendationStatus.h), so checksNotEvaluatedCount does not count it -
    the tier still never reaches PASS here, but for the separate,
    always-true reason that ThermalValidation is always a preliminary estimate
    (see test_case11 below), not because of the missing core loss specifically."""
    result = magnetics_cpp.run_inductor_design(_design_request())
    assert result.status == "ok"
    assert all(c.losses.coreLossStatus == magnetics_cpp.EvaluationStatus.NotEvaluated for c in result.candidates)
    assert all(not c.lossSummary.isCompleteTotal for c in result.candidates)
    assert all("Known Partial Loss" in c.lossSummary.label for c in result.candidates)
    assert all(c.recommendation.tier != magnetics_cpp.RecommendationTier.Pass for c in result.candidates)


@pytest.mark.xfail(
    reason="data/real_core_loss_coefficients.csv is currently empty - Magnetics does not publish "
    "Steinmetz (k/alpha/beta) coefficients for MPP/Kool Mu-family materials the way ferrite vendors did "
    "for the materials this project used to carry, so no candidate can ever reach coreLossStatus == "
    "Evaluated against the current MPP/E-core-only database (see LossEvaluation.cpp's "
    "findCoreLossCoefficients call and this file's own provenance note). Not an engine bug - revisit if "
    "real core-loss coefficients are ever sourced for these materials.",
    strict=False,
)
def test_case11_thermal_stays_preliminary_even_with_full_loss_coverage():
    """Case 11: originally found by probing the live engine - supplying a real ripple
    current (0.5A) alongside the case 9/10 baseline gave at least one real
    candidate both copper AND core loss Evaluated - full known loss coverage -
    yet thermal.status was still only ever PreliminaryThermalEstimate (never a
    "fully evaluated" thermal value exists in Phase 1 - see ThermalEvaluation.h),
    so that candidate still capped at CONDITIONAL_PASS despite having the most
    complete loss data possible in this engine today. (Originally probed against
    E100/60/28-3C90 - that ferrite catalog row has since been removed from
    real_cores.csv along with all ferrite scope. See the xfail reason above for
    why no candidate currently reaches core-loss-Evaluated at all against the
    current MPP/E-core-only database.)"""
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
        assert c.recommendation.tier == magnetics_cpp.RecommendationTier.ConditionalPass


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


def test_case14_mode2_rms_plus_ripple_derives_peak_and_evaluates_saturation():
    """Case 14 (recommendation-confidence round): RMS + ripple supplied, peak current NOT supplied
    (3000uH, 5A RMS, 2A pk-pk ripple, 100kHz) - probed live: every one of the 33 area-product-feasible
    candidates gets a real, derived peak current and SaturationValidation actually evaluates against
    it, proving the Mode 2 peak derivation (RequirementDerivationService.cpp) works end-to-end through
    the full Python binding path, not just the C++ unit tests."""
    result = magnetics_cpp.run_inductor_design(
        _design_request(inductanceUH=3000.0, rmsCurrentA=5.0, rippleCurrentPeakToPeakA=2.0,
                         switchingFreqKHz=100.0, ambientTemperatureC=25.0, allowableTempRiseC=40.0,
                         peakCurrentA=None)
    )
    all_candidates = list(result.candidates) + list(result.rejectedCandidates)
    assert all_candidates, "expected at least one area-product-feasible candidate for this real request"
    saturation_evaluated = [
        c for c in all_candidates
        if any(v.checkName == "SaturationValidation" and v.status.name == "Evaluated" for v in c.validations)
    ]
    assert saturation_evaluated, "SaturationValidation should genuinely evaluate against the derived peak current"


def test_case15_rms_only_reports_missing_peak_as_the_bottleneck():
    """Case 15 (recommendation-confidence round): a genuinely RMS-only request (3000uH, 5A RMS,
    100kHz, no peak, no ripple - the original "Roger" reproduction case) - probed live: 45 of 157
    candidates report bottleneck.reason == NotEvaluatedCheck with limitingCheckName ==
    "SaturationValidation", and designNarrative is the exact required phrase, never a generic
    "increase turns" guess when the real blocker is missing data."""
    result = magnetics_cpp.run_inductor_design(
        _design_request(inductanceUH=3000.0, rmsCurrentA=5.0, switchingFreqKHz=100.0,
                         ambientTemperatureC=25.0, allowableTempRiseC=40.0, peakCurrentA=None)
    )
    assert result.status == "ok"

    all_candidates = list(result.candidates) + list(result.rejectedCandidates)
    not_evaluated_candidates = [c for c in all_candidates if c.bottleneck.reason.name == "NotEvaluatedCheck"]
    assert not_evaluated_candidates, "expected at least one candidate whose bottleneck is missing peak current"

    sample = not_evaluated_candidates[0]
    assert sample.bottleneck.limitingCheckName == "SaturationValidation"
    assert sample.designNarrative == "supply peak current to evaluate saturation risk"
