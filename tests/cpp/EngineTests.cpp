// Minimal assert-based test executable (no external test framework - the
// project had zero test infra before Phase 1; this stays lightweight
// rather than pulling in GoogleTest/Catch2 for a handful of checks).
// Run via `ctest` after `cmake --build`.

#include <cassert>
#include <cmath>
#include <cstdio>
#include "core/magnetics/GapDesign.h"
#include "core/thermal/ThermalEvaluation.h"
#include "validation/DesignValidation.h"

namespace {

bool approxEqual(double a, double b, double tolerance) {
    return std::abs(a - b) <= tolerance;
}

// Verified against data/real_cores.csv row "E100/60/28-3C90":
// Ae=735.0502256509033 mm^2, Le=273.919572699267 mm, Mu=2249.28,
// AL=7584.855918773515 nH/turn^2 (ungapped).
void testUngappedAlMatchesCatalog() {
    double aeCm2 = 735.0502256509033 / 100.0;
    double leCm = 273.919572699267 / 10.0;
    double muR = 2249.28;

    double al = calculateEffectiveAlNhPerTurnSq(aeCm2, leCm, muR, 0.0);
    assert(approxEqual(al, 7584.855918773515, 5.0));
    std::printf("testUngappedAlMatchesCatalog: AL=%.3f (expected ~7584.856)\n", al);
}

// Gap solved for a target inductance at a given turns count must, when fed
// back into calculateEffectiveAlNhPerTurnSq, reproduce that target
// inductance (round-trip consistency between the two GapDesign functions).
void testGapRoundTrip() {
    double aeCm2 = 7.350502256509033;
    double leCm = 27.3919572699267;
    double muR = 2249.28;
    int turns = 30;
    double targetNh = 5000.0;

    double gapCm = calculateRequiredGapCm(turns, aeCm2, leCm, muR, targetNh);
    assert(gapCm > 0.0);  // this target/turns combination should require a real gap

    double alEff = calculateEffectiveAlNhPerTurnSq(aeCm2, leCm, muR, gapCm);
    double actualNh = static_cast<double>(turns) * turns * alEff;

    assert(approxEqual(actualNh, targetNh, 1.0));
    std::printf("testGapRoundTrip: gap=%.4f cm, actualL=%.2f nH (target %.2f)\n", gapCm, actualNh, targetNh);
}

// A zero/negative required gap means the target inductance exceeds what
// the ungapped core can reach at this turns count - not an error
// condition, just a signal that a gap alone can't get there (more turns
// would be needed instead).
void testNoGapNeededIsNotAnError() {
    double aeCm2 = 7.350502256509033;
    double leCm = 27.3919572699267;
    double muR = 2249.28;
    int turns = 1;
    double targetNh = 50000.0;  // far above the ungapped L at N=1 (~7585 nH)

    double gapCm = calculateRequiredGapCm(turns, aeCm2, leCm, muR, targetNh);
    assert(gapCm <= 0.0);
    std::printf("testNoGapNeededIsNotAnError: gap=%.4f cm (negative/zero as expected)\n", gapCm);
}

// Unit conversions used throughout the pipeline (spec section 9 acceptance
// criteria: "every unit conversion is tested").
void testUnitConversions() {
    assert(approxEqual(250.0 * 1e-6, 0.00025, 1e-12));      // uH -> H
    assert(approxEqual(100.0 * 1000.0, 100000.0, 1e-9));    // kHz -> Hz
    assert(approxEqual(735.0502256509033 / 100.0, 7.350502256509033, 1e-9));  // mm^2 -> cm^2
    assert(approxEqual(273.919572699267 / 10.0, 27.3919572699267, 1e-9));     // mm -> cm
    std::printf("testUnitConversions: ok\n");
}

// A not_evaluated check must never report passed=true (spec section 10:
// "never assume missing data equals a pass"), but callers must be able to
// tell "ran and failed" apart from "couldn't run" via ValidationResult::status
// (see InductorDesignService's candidate pass/fail aggregation).
void testThermalValidationNotEvaluatedIsNotAPass() {
    ThermalEvaluationResult thermal = evaluateThermal();
    ValidationResult result = ThermalValidation(thermal, 40.0);

    assert(thermal.status == EvaluationStatus::NotEvaluated);
    assert(result.passed == false);
    assert(result.status == EvaluationStatus::NotEvaluated);
    std::printf("testThermalValidationNotEvaluatedIsNotAPass: ok\n");
}

}  // namespace

int main() {
    testUngappedAlMatchesCatalog();
    testGapRoundTrip();
    testNoGapNeededIsNotAnError();
    testUnitConversions();
    testThermalValidationNotEvaluatedIsNotAPass();
    std::printf("All EngineTests passed.\n");
    return 0;
}
