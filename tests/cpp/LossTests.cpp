#include <cassert>
#include <cmath>
#include <cstdio>
#include "TestHelpers.h"
#include "core/losses/LossEvaluation.h"
#include "core/losses/SkinDepthRisk.h"
#include "data/CoreLossCoefficientDatabase.h"

namespace {

MaterialCandidate testMaterial() {
    MaterialCandidate material;
    material.materialFamily = "TestMat";
    return material;
}

//real Magnetics C055439A2X2 (MPP 60) catalog geometry (data/real_cores.csv) - reused so core-loss volume is
//checked against a real core's Ae/Le, not invented ones. (This replaces the file's old fixture, the
//E100/60/28-3C90 ferrite catalog row, which was removed from real_cores.csv when the database was narrowed
//to Magnetics powder cores only - the loss math this file tests doesn't depend on materialType, so a real
//powder-core geometry works just as well and keeps this suite exercising live catalog data.)
CoreCandidate realCore() {
    CoreCandidate core;
    core.partNumber = "C055439A2X2";
    core.material = "TestMat";
    core.mu = 60.0;
    core.al = 270.0;
    core.aeMm2 = 398.0;
    core.waMm2 = 405.42127530870135;
    core.leMm = 107.0;
    core.mltMm = 100.11000000000001;
    core.areaProductCm4 = 0.0;
    core.meetsAreaProduct = true;
    return core;
}

TurnsAndGapResult convergedTurnsAndGap(int turns, double calculatedInductanceUH) {
    TurnsAndGapResult result;
    result.converged = true;
    result.turns = turns;
    result.calculatedInductanceUH = calculatedInductanceUH;
    return result;
}

WindingDesignResult windingWithKnownDcr(double dcrOhms) {
    WindingDesignResult winding;
    winding.resistanceStatus = EvaluationStatus::Evaluated;
    winding.dcrOhms = dcrOhms;
    return winding;
}

//1. DC copper loss should be evaluated (I^2 * R) exactly when winding.resistanceStatus == Evaluated.
void testCopperLossEvaluatedWhenResistanceKnown() {
    MaterialCandidate material = testMaterial();
    CoreCandidate core = realCore();
    TurnsAndGapResult turnsAndGap = convergedTurnsAndGap(20, 100.0);
    WindingDesignResult winding = windingWithKnownDcr(0.05);

    LossEvaluationResult result = evaluateLosses(material, core, turnsAndGap, winding, 5.0, 100000.0, std::nullopt);
    assert(result.copperLossStatus == EvaluationStatus::Evaluated);
    assert(approxEqual(result.copperLossW, 5.0 * 5.0 * 0.05, 1e-9));
    std::printf("testCopperLossEvaluatedWhenResistanceKnown: copperLossW=%.4f (expected 1.25)\n", result.copperLossW);
}

//2. Copper loss must be honestly not_evaluated - never a silently-invented DCR - when winding resistance
//itself was never evaluated (e.g. no MLT data for the core).
void testCopperLossNotEvaluatedWhenResistanceUnknown() {
    MaterialCandidate material = testMaterial();
    CoreCandidate core = realCore();
    TurnsAndGapResult turnsAndGap = convergedTurnsAndGap(20, 100.0);
    WindingDesignResult winding;  //resistanceStatus defaults to NotEvaluated

    LossEvaluationResult result = evaluateLosses(material, core, turnsAndGap, winding, 5.0, 100000.0, std::nullopt);
    assert(result.copperLossStatus == EvaluationStatus::NotEvaluated);
    assert(!result.missingData.empty());
    std::printf("testCopperLossNotEvaluatedWhenResistanceUnknown: ok\n");
}

//3. Core loss must stay not_evaluated when no ripple current is supplied - flux-density swing must never be
//approximated from peak flux (Option 1, see LossEvaluation.h).
void testCoreLossNotEvaluatedWithoutRippleCurrent() {
    MaterialCandidate material = testMaterial();
    CoreCandidate core = realCore();
    TurnsAndGapResult turnsAndGap = convergedTurnsAndGap(20, 100.0);
    WindingDesignResult winding = windingWithKnownDcr(0.05);

    LossEvaluationResult result = evaluateLosses(material, core, turnsAndGap, winding, 5.0, 100000.0, std::nullopt);
    assert(result.coreLossStatus == EvaluationStatus::NotEvaluated);
    std::printf("testCoreLossNotEvaluatedWithoutRippleCurrent: ok\n");
}

//4. With a real matching Steinmetz coefficient row and a real ripple current, core loss must be evaluated
//using Pv = k * f^alpha * B^beta (W/m^3) times core volume (Ae * Le) - checked against an independent
//hand-computation of the exact same formula, and the detail fields (spec section 7) must be populated.
void testCoreLossEvaluatedWithRealCoefficientsAndRippleCurrent() {
    CoreLossCoefficientData coefficients;
    coefficients.materialName = "TestMat";
    coefficients.minFreqHz = 50000.0;
    coefficients.maxFreqHz = 150000.0;
    coefficients.k = 1.0;
    coefficients.alpha = 1.0;
    coefficients.beta = 2.0;
    CoreLossCoefficientDatabase::setData({coefficients});

    MaterialCandidate material = testMaterial();
    CoreCandidate core = realCore();
    int turns = 20;
    double inductanceUH = 100.0;
    TurnsAndGapResult turnsAndGap = convergedTurnsAndGap(turns, inductanceUH);
    WindingDesignResult winding = windingWithKnownDcr(0.05);
    double rippleA = 2.0;
    double switchingFreqHz = 100000.0;

    LossEvaluationResult result = evaluateLosses(material, core, turnsAndGap, winding, 5.0, switchingFreqHz, rippleA);
    assert(result.coreLossStatus == EvaluationStatus::Evaluated);

    //Independent hand-computation of the exact same formula, mirroring LossEvaluation.cpp's own math.
    double inductanceH = inductanceUH * 1e-6;
    double aeM2 = core.aeMm2 * 1e-6;
    double expectedFluxSwingT = (inductanceH * rippleA) / (static_cast<double>(turns) * aeM2);
    double expectedDensityWPerM3 = coefficients.k * std::pow(switchingFreqHz, coefficients.alpha) * std::pow(expectedFluxSwingT, coefficients.beta);
    double expectedVolumeM3 = (core.aeMm2 * core.leMm) * 1e-9;
    double expectedCoreLossW = expectedDensityWPerM3 * expectedVolumeM3;

    assert(approxEqual(result.coreLossFluxDensitySwingT, expectedFluxSwingT, 1e-9));
    assert(approxEqual(result.coreLossDensityWPerM3, expectedDensityWPerM3, 1e-9));
    assert(approxEqual(result.coreLossVolumeM3, expectedVolumeM3, 1e-12));
    assert(approxEqual(result.coreLossW, expectedCoreLossW, 1e-9));
    assert(result.coreLossMaterialUsed == "TestMat");
    assert(approxEqual(result.coreLossCoefficientMinFreqHz, 50000.0, 1e-9));
    assert(approxEqual(result.coreLossCoefficientMaxFreqHz, 150000.0, 1e-9));
    std::printf("testCoreLossEvaluatedWithRealCoefficientsAndRippleCurrent: coreLossW=%.6f W (hand-computed match)\n", result.coreLossW);
}

//5. A coefficient row with a validated flux-swing range must reject (not_evaluated, never extrapolated
//silently) a design whose flux-density swing falls outside that range - even though the frequency matches.
void testCoreLossFluxSwingOutsideValidatedRangeIsNotEvaluated() {
    CoreLossCoefficientData coefficients;
    coefficients.materialName = "TestMatNarrowRange";
    coefficients.minFreqHz = 50000.0;
    coefficients.maxFreqHz = 150000.0;
    coefficients.k = 1.0;
    coefficients.alpha = 1.0;
    coefficients.beta = 2.0;
    //The real flux swing for this scenario is ~0.0136 T (see test 4 above) - setting the validated
    //minimum far above that forces the guard to trigger.
    coefficients.minFluxSwingT = 0.5;
    coefficients.maxFluxSwingT = 1.0;
    CoreLossCoefficientDatabase::setData({coefficients});

    MaterialCandidate material;
    material.materialFamily = "TestMatNarrowRange";
    CoreCandidate core = realCore();
    TurnsAndGapResult turnsAndGap = convergedTurnsAndGap(20, 100.0);
    WindingDesignResult winding = windingWithKnownDcr(0.05);

    LossEvaluationResult result = evaluateLosses(material, core, turnsAndGap, winding, 5.0, 100000.0, 2.0);
    assert(result.coreLossStatus == EvaluationStatus::NotEvaluated);
    assert(!result.missingData.empty());
    std::printf("testCoreLossFluxSwingOutsideValidatedRangeIsNotEvaluated: %s\n", result.missingData.back().c_str());
}

//6. A flux-density swing beyond the material's own real saturation flux density (BmaxT) must stay
//not_evaluated, even when the coefficient row itself has no minFluxSwingT/maxFluxSwingT bound (the
//current real_core_loss_coefficients.csv never has one - see DATA_FILES.md). Regression test for a real
//bug found via live UI use: candidates whose flux swing genuinely exceeded saturation were reporting
//tens of thousands of watts of "core loss" from an unbounded Steinmetz power-law extrapolation - see
//LossEvaluation.cpp's BmaxT gate, added directly in response.
void testCoreLossNotEvaluatedWhenFluxSwingExceedsMaterialSaturation() {
    CoreLossCoefficientData coefficients;
    coefficients.materialName = "TestMatSaturating";
    coefficients.minFreqHz = 50000.0;
    coefficients.maxFreqHz = 150000.0;
    coefficients.k = 1.0;
    coefficients.alpha = 1.0;
    coefficients.beta = 2.0;
    //No minFluxSwingT/maxFluxSwingT set - this is the realistic case (every material in the
    //current CSV), so fluxSwingWithinValidatedRange() alone would NOT catch this.
    CoreLossCoefficientDatabase::setData({coefficients});

    MaterialCandidate material;
    material.materialFamily = "TestMatSaturating";
    material.hasBmaxData = true;
    material.bmaxT = 0.4;  //real material Bsat, e.g. a typical ferrite

    CoreCandidate core = realCore();
    //Few turns + large ripple current drives a large flux swing - the same real mechanism that
    //triggered the live bug (see LossEvaluation.cpp comment for the exact numbers found).
    int turns = 3;
    double inductanceUH = 250.0;
    TurnsAndGapResult turnsAndGap = convergedTurnsAndGap(turns, inductanceUH);
    WindingDesignResult winding = windingWithKnownDcr(0.05);
    double rippleA = 9.0;
    double switchingFreqHz = 100000.0;

    //Confirm the scenario really does exceed Bsat before asserting on the gate.
    double inductanceH = inductanceUH * 1e-6;
    double aeM2 = core.aeMm2 * 1e-6;
    double fluxSwingT = (inductanceH * rippleA) / (static_cast<double>(turns) * aeM2);
    assert(fluxSwingT > material.bmaxT);

    LossEvaluationResult result = evaluateLosses(material, core, turnsAndGap, winding, 5.0, switchingFreqHz, rippleA);
    assert(result.coreLossStatus == EvaluationStatus::NotEvaluated);
    assert(!result.missingData.empty());
    std::printf("testCoreLossNotEvaluatedWhenFluxSwingExceedsMaterialSaturation: fluxSwingT=%.4f > bmaxT=%.4f, %s\n",
                fluxSwingT, material.bmaxT, result.missingData.back().c_str());
}

//7. A thin strand at a modest switching frequency (radius well below the skin depth) must classify as Low risk.
void testSkinDepthRiskLowForThinStrandAtModestFrequency() {
    DesignRules rules = DesignRules::phase1Default();
    //AWG28 bare area (~0.081 mm^2, radius ~0.16 mm) at 50 kHz.
    SkinDepthRiskResult result = evaluateSkinDepthRisk(50000.0, 0.0810, rules);
    assert(result.riskLevel == AcLossRiskLevel::Low);
    assert(result.radiusToSkinDepthRatio <= rules.skinDepthRiskModerateThreshold);
    assert(result.acLossWattsStatus == EvaluationStatus::NotEvaluated);
    std::printf("testSkinDepthRiskLowForThinStrandAtModestFrequency: ratio=%.4f skinDepth=%.4f mm\n",
                result.radiusToSkinDepthRatio, result.skinDepthMm);
}

//8. A thick strand at a high switching frequency (radius well beyond the skin depth) must classify as High risk.
void testSkinDepthRiskHighForThickStrandAtHighFrequency() {
    DesignRules rules = DesignRules::phase1Default();
    //AWG10 bare area (~5.261 mm^2, radius ~1.29 mm) at 500 kHz.
    SkinDepthRiskResult result = evaluateSkinDepthRisk(500000.0, 5.261, rules);
    assert(result.riskLevel == AcLossRiskLevel::High);
    assert(result.radiusToSkinDepthRatio > rules.skinDepthRiskHighThreshold);
    std::printf("testSkinDepthRiskHighForThickStrandAtHighFrequency: ratio=%.4f skinDepth=%.4f mm\n",
                result.radiusToSkinDepthRatio, result.skinDepthMm);
}

//9. Risk level is always a qualitative heuristic, never a watts figure - acLossWattsStatus must stay
//NotEvaluated regardless of how severe the risk level is.
void testSkinDepthRiskNeverProducesAWattsFigure() {
    DesignRules rules = DesignRules::phase1Default();
    SkinDepthRiskResult lowRisk = evaluateSkinDepthRisk(50000.0, 0.0810, rules);
    SkinDepthRiskResult highRisk = evaluateSkinDepthRisk(500000.0, 5.261, rules);
    assert(lowRisk.acLossWattsStatus == EvaluationStatus::NotEvaluated);
    assert(highRisk.acLossWattsStatus == EvaluationStatus::NotEvaluated);
    assert(!lowRisk.acLossWattsExplanation.empty());
    assert(!highRisk.reason.empty());
    std::printf("testSkinDepthRiskNeverProducesAWattsFigure: ok\n");
}

}  //namespace

void runLossTests() {
    testCopperLossEvaluatedWhenResistanceKnown();
    testCopperLossNotEvaluatedWhenResistanceUnknown();
    testCoreLossNotEvaluatedWithoutRippleCurrent();
    testCoreLossEvaluatedWithRealCoefficientsAndRippleCurrent();
    testCoreLossFluxSwingOutsideValidatedRangeIsNotEvaluated();
    testCoreLossNotEvaluatedWhenFluxSwingExceedsMaterialSaturation();
    testSkinDepthRiskLowForThinStrandAtModestFrequency();
    testSkinDepthRiskHighForThickStrandAtHighFrequency();
    testSkinDepthRiskNeverProducesAWattsFigure();
    std::printf("All LossTests passed.\n");
}
