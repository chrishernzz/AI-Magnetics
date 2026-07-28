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

}  // namespace

void runWindingDesignTests() {
    testSingleStrandSelectedForModerateCurrent();
    testParallelStrandsSelectedForHighCurrent();
    testPhysicalWindowFillIsStricterThanRawCopperFill();
    testColdDcrIncludesLeadRoutingAndConnectionResistance();
    testMissingMltMeansResistanceNotEvaluatedButFillStillComputed();
    testEstimatedHotDcrExceedsColdDcr();
    std::printf("All WindingDesignTests passed.\n");
}
