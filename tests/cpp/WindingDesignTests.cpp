#include <cassert>
#include <cstdio>
#include "TestHelpers.h"
#include "core/winding/WindingDesign.h"

namespace {

//real E100/60/28-3C90 catalog geometry (data/real_cores.csv), including its real Mlt column - reused so
//DCR/length tests are checked against a real core's mean-length-per-turn, not an invented one.
CoreCandidate realCore() {
    CoreCandidate core;
    core.partNumber = "E100/60/28-3C90";
    core.material = "3C90";
    core.mu = 2249.28;
    core.al = 7584.855918773515;
    core.aeMm2 = 735.0502256509033;
    core.waMm2 = 2138.7025;
    core.leMm = 273.919572699267;
    core.mltMm = 110.0;
    core.areaProductCm4 = 0.0;
    core.meetsAreaProduct = true;
    return core;
}

CoreCandidate coreWithoutMlt() {
    CoreCandidate core = realCore();
    core.mltMm = 0.0;
    return core;
}

//same real E100/60/28-3C90 core, with its real rectangular winding-window dimensions added (data/real_cores.csv's
//WindowWidthMm/WindowHeightMm - 22.825mm x 93.7mm, verified against the live PyOpenMagnetics record).
CoreCandidate realCoreWithWindow() {
    CoreCandidate core = realCore();
    core.windowWidthMm = 22.825;
    core.windowHeightMm = 93.7;
    return core;
}

//a synthetic two-piece core with a deliberately narrow window (clearly not a real catalog part - used only
//to force a parallel-strand bundle wider than the opening, exercising the real BundleFitValidation failure
//path deterministically rather than hunting for a real part that happens to be too small).
CoreCandidate twoPieceCoreWithNarrowWindow() {
    CoreCandidate core = realCore();
    core.partNumber = "SYNTHETIC-narrow-window-test-fixture";
    core.windowWidthMm = 2.0;
    core.windowHeightMm = 93.7;
    return core;
}

//1. A small RMS current, needing less copper area than AWG18 (rules.minimumSingleStrandAwg) provides, should
//select a single round strand rather than switching to parallel strands.
void testSingleStrandSelectedForModerateCurrent() {
    CoreCandidate core = realCore();
    DesignRules rules = DesignRules::phase1Default();
    WindingDesignResult result = designWinding(core, 20, 2.0, rules);
    assert(result.parallelStrands == 1);
    assert(result.constructionType == WindingConstructionType::SingleRoundWire);
    std::printf("testSingleStrandSelectedForModerateCurrent: %s\n", result.wireDescription.c_str());
}

//2. A large RMS current (implying a conductor coarser than rules.minimumSingleStrandAwg) must switch to
//parallel strands of the minimum practical gauge rather than one impractically thick solid wire.
void testParallelStrandsSelectedForHighCurrent() {
    CoreCandidate core = realCore();
    DesignRules rules = DesignRules::phase1Default();
    WindingDesignResult result = designWinding(core, 20, 60.0, rules);
    assert(result.parallelStrands > 1);
    assert(result.constructionType == WindingConstructionType::ParallelRoundWires);
    assert(result.bundleFitStatus == EvaluationStatus::NotEvaluated);
    assert(!result.missingData.empty());
    std::printf("testParallelStrandsSelectedForHighCurrent: %s (%d strands)\n", result.wireDescription.c_str(), result.parallelStrands);
}

//3. The realistic physical window fill (insulation, packing factor, bobbin/margin/lead-exit derates) must be
//stricter (higher) than the raw copper-only fill factor for the same design - the whole point of the model.
void testPhysicalWindowFillIsStricterThanRawCopperFill() {
    CoreCandidate core = realCore();
    DesignRules rules = DesignRules::phase1Default();
    WindingDesignResult result = designWinding(core, 20, 5.0, rules);
    assert(result.physicalWindowFillFactor > result.fillFactor);
    std::printf("testPhysicalWindowFillIsStricterThanRawCopperFill: raw=%.4f physical=%.4f\n",
                result.fillFactor, result.physicalWindowFillFactor);
}

//4. coldDcrOhmsAt20C must include lead/routing/connection resistance on top of the core-winding-only
//resistance - i.e. it must be strictly greater than what the old core-winding-only DCR would have been.
void testColdDcrIncludesLeadRoutingAndConnectionResistance() {
    CoreCandidate core = realCore();
    DesignRules rules = DesignRules::phase1Default();
    WindingDesignResult result = designWinding(core, 20, 5.0, rules);
    assert(result.resistanceStatus == EvaluationStatus::Evaluated);

    // Hand-computed core-winding-only resistance (the pre-Commit-8 formula), using the same real resistivity
    // constant WindingDesign.cpp uses internally.
    constexpr double kCopperResistivityOhmMAt20C = 1.724e-8;
    double conductorAreaM2 = result.conductorAreaMm2 * 1e-6;
    double coreWindingOnlyResistanceOhms =
        (kCopperResistivityOhmMAt20C * result.coreWindingLengthM / conductorAreaM2) / result.parallelStrands;

    assert(result.coldDcrOhmsAt20C > coreWindingOnlyResistanceOhms);
    assert(result.leadLengthM > 0.0);
    assert(result.routingLengthM > 0.0);
    assert(result.connectionResistanceOhms > 0.0);
    assert(approxEqual(result.totalLengthM, result.coreWindingLengthM + result.leadLengthM + result.routingLengthM, 1e-9));
    std::printf("testColdDcrIncludesLeadRoutingAndConnectionResistance: coreWindingOnly=%.6f ohm, full=%.6f ohm\n",
                coreWindingOnlyResistanceOhms, result.coldDcrOhmsAt20C);
}

//5. A core with no MLT data must report resistanceStatus=NotEvaluated (never an invented DCR) - but the
//physical-fill fields, which don't depend on MLT, must still be computed.
void testMissingMltMeansResistanceNotEvaluatedButFillStillComputed() {
    CoreCandidate core = coreWithoutMlt();
    DesignRules rules = DesignRules::phase1Default();
    WindingDesignResult result = designWinding(core, 20, 5.0, rules);
    assert(result.resistanceStatus == EvaluationStatus::NotEvaluated);
    assert(approxEqual(result.coldDcrOhmsAt20C, 0.0, 1e-12));
    assert(result.physicalWindowFillFactor > 0.0);
    assert(!result.physicalDescription.empty());
    std::printf("testMissingMltMeansResistanceNotEvaluatedButFillStillComputed: ok\n");
}

//6. estimatedHotDcrOhms is a conservative sanity-check estimate at rules.assumedWindingTempCWhenThermalNotEvaluated
//(90C default) - since copper's temp coefficient is positive and 90C > 20C, it must exceed the cold DCR.
void testEstimatedHotDcrExceedsColdDcr() {
    CoreCandidate core = realCore();
    DesignRules rules = DesignRules::phase1Default();
    WindingDesignResult result = designWinding(core, 20, 5.0, rules);
    assert(result.estimatedHotDcrOhms > result.coldDcrOhmsAt20C);
    assert(approxEqual(result.dcrOhms, result.coldDcrOhmsAt20C, 1e-12));
    std::printf("testEstimatedHotDcrExceedsColdDcr: cold=%.6f ohm, hot(estimate)=%.6f ohm\n",
                result.coldDcrOhmsAt20C, result.estimatedHotDcrOhms);
}

//7. Regression for a real database audit finding: real winding-window width/height exists for two-piece
//cores but was never used. A parallel-strand bundle that comfortably fits within the core's real narrowest
//window opening must report bundleFitStatus=Evaluated and bundleFitsWindowOpening=true.
void testBundleFitsRealWindowOpening() {
    CoreCandidate core = realCoreWithWindow();
    DesignRules rules = DesignRules::phase1Default();
    WindingDesignResult result = designWinding(core, 20, 60.0, rules);
    assert(result.parallelStrands > 1);
    assert(result.bundleFitStatus == EvaluationStatus::Evaluated);
    assert(result.bundleWidthMm > 0.0);
    assert(approxEqual(result.narrowestWindowOpeningMm, 22.825, 1e-9));
    assert(result.bundleFitsWindowOpening);
    std::printf("testBundleFitsRealWindowOpening: bundleWidth=%.4f mm vs opening=%.4f mm - fits\n",
                result.bundleWidthMm, result.narrowestWindowOpeningMm);
}

//8. A parallel-strand bundle wider than the core's narrowest real window opening must report
//bundleFitStatus=Evaluated and bundleFitsWindowOpening=false - a genuine geometric impossibility, not
//silently assumed benign.
void testBundleTooWideForNarrowWindowFailsRealCheck() {
    CoreCandidate core = twoPieceCoreWithNarrowWindow();
    DesignRules rules = DesignRules::phase1Default();
    WindingDesignResult result = designWinding(core, 20, 60.0, rules);
    assert(result.parallelStrands > 1);
    assert(result.bundleFitStatus == EvaluationStatus::Evaluated);
    assert(result.bundleWidthMm > result.narrowestWindowOpeningMm);
    assert(!result.bundleFitsWindowOpening);
    std::printf("testBundleTooWideForNarrowWindowFailsRealCheck: bundleWidth=%.4f mm vs opening=%.4f mm - does not fit\n",
                result.bundleWidthMm, result.narrowestWindowOpeningMm);
}

//real Magnetics C055439A2X2 (MPP 60) - a genuine powder toroid from real_cores.csv, the exact core a real
//user report (a senior magnetics engineer's hand calculation) was filed against.
CoreCandidate realPowderToroid() {
    CoreCandidate core;
    core.partNumber = "C055439A2X2";
    core.material = "MPP 60";
    core.mu = 60.0;
    core.al = 316.9118489299326;
    core.aeMm2 = 449.4919;
    core.waMm2 = 419.82233603275705;
    core.leMm = 106.94106558574245;
    core.mltMm = 98.27;
    core.coreShape = "Toroid";
    core.materialType = "powder";
    return core;
}

//9. Regression for a real user report: rules.bobbinWindowDerateFactor models a physical bobbin former, which
//a Toroid does not have - applying it anyway overstated how much window space a toroid loses. For the exact
//real core/turns/current combination the report was filed against, removing the inapplicable bobbin derate
//(margin/lead-exit clearance still applies) must make physicalWindowFillFactor LOWER (less strict) than it
//was before this fix, by exactly the 1/0.85 factor the derate contributed - not an approximation.
void testToroidSkipsInapplicableBobbinDerate() {
    CoreCandidate toroidCore = realPowderToroid();
    CoreCandidate twoPieceCore = realPowderToroid();
    twoPieceCore.coreShape = "TwoPieceSet";  // same real core geometry, hypothetically bobbin-wound
    DesignRules rules = DesignRules::phase1Default();

    WindingDesignResult toroidResult = designWinding(toroidCore, 166, 5.0, rules);
    WindingDesignResult twoPieceResult = designWinding(twoPieceCore, 166, 5.0, rules);

    assert(toroidResult.physicalWindowFillFactor < twoPieceResult.physicalWindowFillFactor);
    // exact ratio: the two-piece result still carries the 0.85 bobbin derate the toroid now skips
    assert(approxEqual(toroidResult.physicalWindowFillFactor, twoPieceResult.physicalWindowFillFactor * rules.bobbinWindowDerateFactor, 1e-6));
    std::printf("testToroidSkipsInapplicableBobbinDerate: toroid=%.4f vs hypothetical-bobbin-wound=%.4f (ratio matches bobbinWindowDerateFactor=%.2f exactly)\n",
                toroidResult.physicalWindowFillFactor, twoPieceResult.physicalWindowFillFactor, rules.bobbinWindowDerateFactor);
}

}  // namespace

void runWindingDesignTests() {
    testSingleStrandSelectedForModerateCurrent();
    testParallelStrandsSelectedForHighCurrent();
    testPhysicalWindowFillIsStricterThanRawCopperFill();
    testColdDcrIncludesLeadRoutingAndConnectionResistance();
    testMissingMltMeansResistanceNotEvaluatedButFillStillComputed();
    testEstimatedHotDcrExceedsColdDcr();
    testBundleFitsRealWindowOpening();
    testBundleTooWideForNarrowWindowFailsRealCheck();
    testToroidSkipsInapplicableBobbinDerate();
    std::printf("All WindingDesignTests passed.\n");
}
