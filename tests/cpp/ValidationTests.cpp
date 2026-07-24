#include <cassert>
#include <cstdio>
#include "TestHelpers.h"
#include "validation/DesignValidation.h"

namespace {

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

CoreCandidate testCore() {
    CoreCandidate core;
    core.partNumber = "E100/60/28-3C90";
    core.material = "3C90";
    core.mu = 2249.28;
    core.al = 7584.855918773515;
    core.aeMm2 = 735.0502256509033;
    core.waMm2 = 2020.0;
    core.leMm = 273.919572699267;
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

//6. PeakFluxValidation/SaturationValidation flag usedDefaultLimit when the material has no BmaxT - the same
//"never silently present a default as fact" rule FluxLimitTiers applies above.
void testPeakFluxValidationFlagsDefaultLimit() {
    CoreCandidate core = testCore();
    MaterialCandidate material = materialWithoutBmax();
    DesignRules rules = DesignRules::phase1Default();
    TurnsAndGapResult turnsAndGap = convergedTurnsAndGap(20, 2.0, 0.1);

    ValidationResult result = PeakFluxValidation(core, material, turnsAndGap, 5.0, rules);
    assert(result.usedDefaultLimit);
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

    ValidationResult lowCurrent = SaturationValidation(core, material, turnsAndGap, 2.0, rules);
    ValidationResult highCurrent = SaturationValidation(core, material, turnsAndGap, 20.0, rules);
    assert(highCurrent.calculatedValue < lowCurrent.calculatedValue);
    std::printf("testSaturationValidationMarginShrinksWithHigherPeakCurrent: margin@2A=%.2f%% margin@20A=%.2f%%\n",
                lowCurrent.calculatedValue, highCurrent.calculatedValue);
}

//8. WindingFitValidation currently gates on raw copper fill (winding.fitsWindow) - this is the Phase 1 behavior
//before the physical-fill model lands; that commit will update this test alongside the gate it changes.
void testWindingFitValidationGatesOnFitsWindow() {
    DesignRules rules = DesignRules::phase1Default();
    WindingDesignResult winding;
    winding.fillFactor = 0.3;
    winding.fitsWindow = true;
    ValidationResult passResult = WindingFitValidation(winding, rules);
    assert(passResult.passed);

    winding.fitsWindow = false;
    ValidationResult failResult = WindingFitValidation(winding, rules);
    assert(!failResult.passed);
    std::printf("testWindingFitValidationGatesOnFitsWindow: ok\n");
}

//9. CurrentDensityValidation compares against the allowable density converted to the same A/mm^2 unit winding
//already reports in - this is a direct regression check on that unit conversion, not just the comparison logic.
void testCurrentDensityValidationConvertsAllowableToAPerMm2() {
    DesignRules rules = DesignRules::phase1Default();
    WindingDesignResult winding;
    winding.currentDensityAperMm2 = rules.allowableCurrentDensityAperCm2 / 100.0;  // exactly at the allowable limit
    ValidationResult result = CurrentDensityValidation(winding, rules);
    assert(result.passed);
    assert(approxEqual(result.limitValue, rules.allowableCurrentDensityAperCm2 / 100.0, 1e-9));
    std::printf("testCurrentDensityValidationConvertsAllowableToAPerMm2: limit=%.6f A/mm^2\n", result.limitValue);
}

//10. ThermalValidation must never report passed=true when thermal.status == NotEvaluated - spec section 10's
//"never assume missing data equals a pass," exercised directly against today's real evaluateThermal() output.
void testThermalValidationNotEvaluatedIsNeverAPass() {
    ThermalEvaluationResult thermal = evaluateThermal();
    ValidationResult result = ThermalValidation(thermal, 40.0);
    assert(thermal.status == EvaluationStatus::NotEvaluated);
    assert(!result.passed);
    assert(result.status == EvaluationStatus::NotEvaluated);
    std::printf("testThermalValidationNotEvaluatedIsNeverAPass: ok\n");
}

//11. When a thermal result IS Evaluated (constructed directly here, since evaluateThermal() itself never
//produces one in Phase 1), ThermalValidation must compare the real numbers rather than falling back to not_evaluated.
void testThermalValidationComparesRealNumbersWhenEvaluated() {
    ThermalEvaluationResult thermal;
    thermal.status = EvaluationStatus::Evaluated;
    thermal.predictedTempRiseC = 25.0;
    ValidationResult passResult = ThermalValidation(thermal, 40.0);
    assert(passResult.passed);
    assert(passResult.status == EvaluationStatus::Evaluated);

    thermal.predictedTempRiseC = 55.0;
    ValidationResult failResult = ThermalValidation(thermal, 40.0);
    assert(!failResult.passed);
    std::printf("testThermalValidationComparesRealNumbersWhenEvaluated: ok\n");
}

}  // namespace

void runValidationTests() {
    testFluxLimitTiersUsesMaterialBmaxWhenAvailable();
    testFluxLimitTiersFallsBackToDefaultWhenNoBmaxData();
    testFluxLimitTiersTemperatureAndCoreLossTiersAlwaysNotEvaluated();
    testInductanceValidationPassesWithinTolerance();
    testInductanceValidationFailsWhenNotConverged();
    testPeakFluxValidationFlagsDefaultLimit();
    testSaturationValidationMarginShrinksWithHigherPeakCurrent();
    testWindingFitValidationGatesOnFitsWindow();
    testCurrentDensityValidationConvertsAllowableToAPerMm2();
    testThermalValidationNotEvaluatedIsNeverAPass();
    testThermalValidationComparesRealNumbersWhenEvaluated();
    std::printf("All ValidationTests passed.\n");
}
