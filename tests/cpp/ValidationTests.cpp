#include <cassert>
#include <cstdio>
#include <vector>
#include "TestHelpers.h"
#include "validation/DesignValidation.h"
#include "validation/RecommendationStatus.h"

namespace {

ValidationResult passingCheck(const char* name) {
    ValidationResult v;
    v.checkName = name;
    v.passed = true;
    v.status = EvaluationStatus::Evaluated;
    return v;
}

SkinDepthRiskResult lowRisk() {
    SkinDepthRiskResult r;
    r.riskLevel = AcLossRiskLevel::Low;
    return r;
}

MaterialCandidate materialWithBmax(double bmaxT) {
    MaterialCandidate material;
    material.materialFamily = "3C90";
    material.hasBmaxData = true;
    material.bmaxT = bmaxT;
    return material;
}

MaterialCandidate materialWithoutBmax() {
    MaterialCandidate material;
    material.materialFamily = "unknown-material";
    material.hasBmaxData = false;
    return material;
}

//real Magnetics C055439A2X2 (MPP 60) catalog geometry (data/real_cores.csv). This replaces the file's old
//fixture, the E100/60/28-3C90 ferrite catalog row, which was removed from real_cores.csv when the database
//was narrowed to Magnetics powder cores only - PeakFluxValidation/SaturationValidation only care about
//Ae/Le, not materialType, so a real powder-core geometry works just as well and keeps this suite exercising
//live catalog data.
CoreCandidate testCore() {
    CoreCandidate core;
    core.partNumber = "C055439A2X2";
    core.material = "MPP 60";
    core.mu = 60.0;
    core.al = 270.0;
    core.aeMm2 = 398.0;
    core.waMm2 = 405.42127530870135;
    core.leMm = 107.0;
    core.areaProductCm4 = 0.0;
    core.meetsAreaProduct = true;
    return core;
}

TurnsAndGapResult convergedTurnsAndGap(int turns, double calculatedInductanceUH, double errorPercent) {
    TurnsAndGapResult result;
    result.converged = true;
    result.withinTolerance = std::abs(errorPercent) <= 10.0;
    result.turns = turns;
    result.calculatedInductanceUH = calculatedInductanceUH;
    result.inductanceErrorPercent = errorPercent;
    //PeakFluxValidation/SaturationValidation read loadedInductanceUH (the real operating-current value),
    //not calculatedInductanceUH (the zero-bias design value) - see DesignValidation.cpp. None of this
    //file's fixtures model DC-bias roll-off, so both are the same real number.
    result.loadedInductanceUH = calculatedInductanceUH;
    return result;
}

//1. FluxLimitTiers should reuse the material's real BmaxT when the material has it, not fall back to the default.
void testFluxLimitTiersUsesMaterialBmaxWhenAvailable() {
    MaterialCandidate material = materialWithBmax(0.47);
    DesignRules rules = DesignRules::phase1Default();

    FluxLimitTiers tiers = calculateFluxLimitTiers(material, rules);

    assert(approxEqual(tiers.absoluteSaturationT, 0.47, 1e-9));
    assert(!tiers.absoluteSaturationIsDefault);
    assert(approxEqual(tiers.recommendedOperatingT, 0.47 * rules.recommendedFluxDerateFactor, 1e-9));
    std::printf("testFluxLimitTiersUsesMaterialBmaxWhenAvailable: absoluteSaturationT=%.4f recommendedOperatingT=%.4f\n",
                tiers.absoluteSaturationT, tiers.recommendedOperatingT);
}

//2. Without material-specific BmaxT, FluxLimitTiers must fall back to the Phase 1 default and flag it as such -
//never silently present a default as if it were measured data.
void testFluxLimitTiersFallsBackToDefaultWhenNoBmaxData() {
    MaterialCandidate material = materialWithoutBmax();
    DesignRules rules = DesignRules::phase1Default();

    FluxLimitTiers tiers = calculateFluxLimitTiers(material, rules);

    assert(approxEqual(tiers.absoluteSaturationT, rules.defaultFluxDensityLimitT, 1e-9));
    assert(tiers.absoluteSaturationIsDefault);
    std::printf("testFluxLimitTiersFallsBackToDefaultWhenNoBmaxData: absoluteSaturationT=%.4f (default)\n",
                tiers.absoluteSaturationT);
}

//3. The temperature-adjusted and core-loss-limited tiers are permanently not_evaluated in Phase 1 (no data exists
//to compute either from) - this must hold regardless of which material/rules are passed in.
void testFluxLimitTiersTemperatureAndCoreLossTiersAlwaysNotEvaluated() {
    DesignRules rules = DesignRules::phase1Default();
    FluxLimitTiers withBmax = calculateFluxLimitTiers(materialWithBmax(0.35), rules);
    FluxLimitTiers withoutBmax = calculateFluxLimitTiers(materialWithoutBmax(), rules);

    assert(withBmax.temperatureAdjustedStatus == EvaluationStatus::NotEvaluated);
    assert(withBmax.coreLossLimitedStatus == EvaluationStatus::NotEvaluated);
    assert(withoutBmax.temperatureAdjustedStatus == EvaluationStatus::NotEvaluated);
    assert(withoutBmax.coreLossLimitedStatus == EvaluationStatus::NotEvaluated);
    std::printf("testFluxLimitTiersTemperatureAndCoreLossTiersAlwaysNotEvaluated: ok\n");
}

//4. InductanceValidation passes only when turns/gap converged and landed within tolerance.
void testInductanceValidationPassesWithinTolerance() {
    TurnsAndGapResult turnsAndGap = convergedTurnsAndGap(10, 100.5, 0.5);
    ValidationResult result = InductanceValidation(turnsAndGap, 10.0);
    assert(result.passed);
    assert(approxEqual(result.calculatedValue, 0.5, 1e-9));
    std::printf("testInductanceValidationPassesWithinTolerance: ok\n");
}

//5. InductanceValidation fails honestly (not a crash, not a silent pass) when turns/gap never converged.
void testInductanceValidationFailsWhenNotConverged() {
    TurnsAndGapResult turnsAndGap;
    turnsAndGap.converged = false;
    ValidationResult result = InductanceValidation(turnsAndGap, 10.0);
    assert(!result.passed);
    std::printf("testInductanceValidationFailsWhenNotConverged: ok\n");
}

//6. PeakFluxValidation/SaturationValidation flag usesDefaultAssumption when the material has no BmaxT - the same
//"never silently present a default as fact" rule FluxLimitTiers applies above.
void testPeakFluxValidationFlagsDefaultLimit() {
    CoreCandidate core = testCore();
    MaterialCandidate material = materialWithoutBmax();
    DesignRules rules = DesignRules::phase1Default();
    TurnsAndGapResult turnsAndGap = convergedTurnsAndGap(20, 2.0, 0.1);

    ValidationResult result = PeakFluxValidation(core, material, turnsAndGap, 5.0, 0.0, rules);
    assert(result.usesDefaultAssumption);
    assert(approxEqual(result.limitValue, rules.defaultFluxDensityLimitT, 1e-9));
    std::printf("testPeakFluxValidationFlagsDefaultLimit: Bpk=%.6f T vs default limit %.4f T\n",
                result.calculatedValue, result.limitValue);
}

//7. SaturationValidation's margin must shrink as peak current rises toward the flux limit.
void testSaturationValidationMarginShrinksWithHigherPeakCurrent() {
    CoreCandidate core = testCore();
    MaterialCandidate material = materialWithBmax(0.47);
    DesignRules rules = DesignRules::phase1Default();
    TurnsAndGapResult turnsAndGap = convergedTurnsAndGap(20, 2.0, 0.1);

    ValidationResult lowCurrent = SaturationValidation(core, material, turnsAndGap, 2.0, 0.0, rules);
    ValidationResult highCurrent = SaturationValidation(core, material, turnsAndGap, 20.0, 0.0, rules);
    assert(highCurrent.calculatedValue < lowCurrent.calculatedValue);
    std::printf("testSaturationValidationMarginShrinksWithHigherPeakCurrent: margin@2A=%.2f%% margin@20A=%.2f%%\n",
                lowCurrent.calculatedValue, highCurrent.calculatedValue);
}

//8. WindingFitValidation gates on the realistic physical window fill (fitsPhysicalWindow), not the raw
//copper-only fill (fitsWindow) - a candidate can pass on raw copper fill alone yet still fail here, which is
//exactly the intentional behavior change the physical-fill model introduces.
void testWindingFitValidationGatesOnPhysicalFillNotRawFill() {
    DesignRules rules = DesignRules::phase1Default();
    WindingDesignResult winding;
    winding.fillFactor = 0.3;
    winding.fitsWindow = true;
    winding.physicalWindowFillFactor = 0.9;
    winding.fitsPhysicalWindow = false;

    ValidationResult result = WindingFitValidation(winding, rules, true);
    assert(!result.passed);
    assert(approxEqual(result.calculatedValue, 0.9, 1e-9));
    std::printf("testWindingFitValidationGatesOnPhysicalFillNotRawFill: raw fill=%.2f (would pass) physical fill=%.2f (fails)\n",
                winding.fillFactor, winding.physicalWindowFillFactor);
}

//9. CurrentDensityValidation compares against the allowable density converted to the same A/mm^2 unit winding
//already reports in - this is a direct regression check on that unit conversion, not just the comparison logic.
void testCurrentDensityValidationConvertsAllowableToAPerMm2() {
    DesignRules rules = DesignRules::phase1Default();
    WindingDesignResult winding;
    winding.currentDensityAperMm2 = rules.allowableCurrentDensityAperCm2 / 100.0;  //exactly at the allowable limit
    ValidationResult result = CurrentDensityValidation(winding, rules);
    assert(result.passed);
    assert(approxEqual(result.limitValue, rules.allowableCurrentDensityAperCm2 / 100.0, 1e-9));
    std::printf("testCurrentDensityValidationConvertsAllowableToAPerMm2: limit=%.6f A/mm^2\n", result.limitValue);
}

//9b. BundleFitValidation must be NotEvaluated when WindingDesign never computed a real bundle-fit check
//(bundleFitStatus defaults to NotEvaluated - single-strand winding, or a core with no real window
//width/height), and must genuinely pass/fail on the real numbers once WindingDesign has computed it.
void testBundleFitValidationNotEvaluatedByDefault() {
    WindingDesignResult winding;  //bundleFitStatus defaults to NotEvaluated
    ValidationResult result = BundleFitValidation(winding);
    assert(result.status == EvaluationStatus::NotEvaluated);
    assert(!result.passed);
    std::printf("testBundleFitValidationNotEvaluatedByDefault: %s\n", result.explanation.c_str());
}

void testBundleFitValidationFailsWhenBundleWiderThanOpening() {
    WindingDesignResult winding;
    winding.parallelStrands = 5;
    winding.bundleFitStatus = EvaluationStatus::Evaluated;
    winding.bundleWidthMm = 10.0;
    winding.narrowestWindowOpeningMm = 6.0;
    winding.bundleFitsWindowOpening = false;

    ValidationResult result = BundleFitValidation(winding);
    assert(result.status == EvaluationStatus::Evaluated);
    assert(!result.passed);
    assert(approxEqual(result.calculatedValue, 10.0, 1e-9));
    assert(approxEqual(result.limitValue, 6.0, 1e-9));
    std::printf("testBundleFitValidationFailsWhenBundleWiderThanOpening: bundle=%.1fmm vs opening=%.1fmm\n",
                result.calculatedValue, result.limitValue);
}

//10. ThermalValidation must never report passed=true when thermal.status == NotEvaluated - spec section 10's
//"never assume missing data equals a pass," exercised directly against a real evaluateThermal() call with
//no known winding geometry (the case that still reports NotEvaluated after the Commit 10 rewrite).
void testThermalValidationNotEvaluatedIsNeverAPass() {
    DesignRules rules = DesignRules::phase1Default();
    ThermalIterationInputs inputs;
    inputs.copperLossGeometryKnown = false;
    ThermalEvaluationResult thermal = evaluateThermal(inputs, rules);
    ValidationResult result = ThermalValidation(thermal, 40.0, true);
    assert(thermal.status == ThermalStatus::NotEvaluated);
    assert(!result.passed);
    assert(result.status == EvaluationStatus::NotEvaluated);
    assert(!result.isPreliminaryEstimate);
    std::printf("testThermalValidationNotEvaluatedIsNeverAPass: ok\n");
}

//11. When a thermal result IS a converged PreliminaryThermalEstimate (a real evaluateThermal() call, not a
//hand-constructed stand-in), ThermalValidation must compare the real numbers, mark isPreliminaryEstimate,
//and still report EvaluationStatus::Evaluated - the check genuinely ran and passed/failed.
void testThermalValidationComparesRealNumbersWhenEvaluated() {
    DesignRules rules = DesignRules::phase1Default();
    ThermalIterationInputs inputs;
    inputs.ambientTemperatureC = 25.0;
    inputs.rmsCurrentA = 1.0;
    inputs.coldDcrOhmsAt20C = 0.01;
    inputs.copperLossGeometryKnown = true;
    ThermalEvaluationResult thermal = evaluateThermal(inputs, rules);
    assert(thermal.status == ThermalStatus::PreliminaryThermalEstimate);

    ValidationResult passResult = ThermalValidation(thermal, 40.0, true);
    assert(passResult.passed);
    assert(passResult.status == EvaluationStatus::Evaluated);
    assert(passResult.isPreliminaryEstimate);

    ValidationResult failResult = ThermalValidation(thermal, 0.001, true);
    assert(!failResult.passed);
    assert(failResult.isPreliminaryEstimate);
    std::printf("testThermalValidationComparesRealNumbersWhenEvaluated: ok\n");
}

//12. classifyRecommendation's tier must always mirror passed==false as Rejected, regardless of what the
//validations themselves say - it never overrides the existing pass/fail decision.
void testDetermineRecommendationStatusRejectMirrorsPassedFalse() {
    std::vector<ValidationResult> validations = {passingCheck("A"), passingCheck("B")};
    RecommendationClassification result = determineRecommendationStatus(false, validations, lowRisk());
    assert(result.tier == RecommendationTier::Reject);
    std::printf("testDetermineRecommendationStatusRejectMirrorsPassedFalse: ok\n");
}

//13. A passed candidate with any not_evaluated check must cap at ConditionalPass, and that check must
//be named in missingInfo.
void testDetermineRecommendationStatusConditionalPassWhenAnyCheckNotEvaluated() {
    ValidationResult notEvaluated;
    notEvaluated.checkName = "ThermalValidation";
    notEvaluated.status = EvaluationStatus::NotEvaluated;
    notEvaluated.explanation = "no thermal data";
    std::vector<ValidationResult> validations = {passingCheck("A"), notEvaluated};

    RecommendationClassification result = determineRecommendationStatus(true, validations, lowRisk());
    assert(result.tier == RecommendationTier::ConditionalPass);
    assert(result.checksNotEvaluatedCount == 1);
    assert(result.checksEvaluatedCount == 1);
    assert(result.missingInfo.size() == 1);
    std::printf("testDetermineRecommendationStatusConditionalPassWhenAnyCheckNotEvaluated: %s\n", result.explanation.c_str());
}

//14. A passed candidate where every check evaluated and passed, but one is flagged isPreliminaryEstimate
//(the real ThermalValidation case), must still cap at ConditionalPass - a real pass/fail on a coarse
//estimate is not the same as a real pass/fail on validated data.
void testDetermineRecommendationStatusConditionalPassWhenAnyPreliminaryEstimate() {
    ValidationResult preliminary = passingCheck("ThermalValidation");
    preliminary.isPreliminaryEstimate = true;
    std::vector<ValidationResult> validations = {passingCheck("A"), preliminary};

    RecommendationClassification result = determineRecommendationStatus(true, validations, lowRisk());
    assert(result.tier == RecommendationTier::ConditionalPass);
    assert(result.checksNotEvaluatedCount == 0);
    std::printf("testDetermineRecommendationStatusConditionalPassWhenAnyPreliminaryEstimate: %s\n", result.explanation.c_str());
}

//15. A passed candidate with an otherwise-clean check list must still cap at ConditionalPass when
//AC-loss risk is Moderate or High - never silently ignored just because every OTHER check passed cleanly.
void testDetermineRecommendationStatusConditionalPassWhenAcRiskModerateOrHigh() {
    std::vector<ValidationResult> validations = {passingCheck("A"), passingCheck("B")};

    SkinDepthRiskResult moderate;
    moderate.riskLevel = AcLossRiskLevel::Moderate;
    RecommendationClassification moderateResult = determineRecommendationStatus(true, validations, moderate);
    assert(moderateResult.tier == RecommendationTier::ConditionalPass);

    SkinDepthRiskResult high;
    high.riskLevel = AcLossRiskLevel::High;
    RecommendationClassification highResult = determineRecommendationStatus(true, validations, high);
    assert(highResult.tier == RecommendationTier::ConditionalPass);
    std::printf("testDetermineRecommendationStatusConditionalPassWhenAcRiskModerateOrHigh: ok\n");
}

//16. Only when passed, every check Evaluated+passed+non-preliminary, AND AC-loss risk is Low does
//classifyRecommendation itself produce Pass - this is a pure unit test of the classifier
//function in isolation. In the actual pipeline this tier is currently unreachable because ThermalValidation
//always sets isPreliminaryEstimate=true (see RecommendationStatus.h) - that is an integration-level fact
//about today's data, not a bug in this function, which correctly reaches the top tier when given
//consistent, ideal inputs.
void testDetermineRecommendationStatusPassWhenEverythingClean() {
    std::vector<ValidationResult> validations = {passingCheck("A"), passingCheck("B"), passingCheck("C")};
    RecommendationClassification result = determineRecommendationStatus(true, validations, lowRisk());
    assert(result.tier == RecommendationTier::Pass);
    assert(result.checksEvaluatedCount == 3);
    assert(result.checksPassedCount == 3);
    assert(result.checksFailedCount == 0);
    std::printf("testDetermineRecommendationStatusPassWhenEverythingClean: %s\n", result.explanation.c_str());
}

}  //namespace

void runValidationTests() {
    testFluxLimitTiersUsesMaterialBmaxWhenAvailable();
    testFluxLimitTiersFallsBackToDefaultWhenNoBmaxData();
    testFluxLimitTiersTemperatureAndCoreLossTiersAlwaysNotEvaluated();
    testInductanceValidationPassesWithinTolerance();
    testInductanceValidationFailsWhenNotConverged();
    testPeakFluxValidationFlagsDefaultLimit();
    testSaturationValidationMarginShrinksWithHigherPeakCurrent();
    testWindingFitValidationGatesOnPhysicalFillNotRawFill();
    testCurrentDensityValidationConvertsAllowableToAPerMm2();
    testBundleFitValidationNotEvaluatedByDefault();
    testBundleFitValidationFailsWhenBundleWiderThanOpening();
    testThermalValidationNotEvaluatedIsNeverAPass();
    testThermalValidationComparesRealNumbersWhenEvaluated();
    testDetermineRecommendationStatusRejectMirrorsPassedFalse();
    testDetermineRecommendationStatusConditionalPassWhenAnyCheckNotEvaluated();
    testDetermineRecommendationStatusConditionalPassWhenAnyPreliminaryEstimate();
    testDetermineRecommendationStatusConditionalPassWhenAcRiskModerateOrHigh();
    testDetermineRecommendationStatusPassWhenEverythingClean();
    std::printf("All ValidationTests passed.\n");
}
