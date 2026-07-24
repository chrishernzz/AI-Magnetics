#include <cassert>
#include <cstdio>
#include "core/thermal/ThermalEvaluation.h"
#include "validation/DesignValidation.h"

//prototypes
void runGapDesignTests();
void runUnitConversionTests();
void runAreaProductTests();
void runGapToleranceTests();

int main() {
    //run all tests in sequence (in order). If every assert in every test passes, prints the final sucess line
    //if any assert anywhere fails, the process aborts at that point and the print is never printed out
    runGapDesignTests();
    runUnitConversionTests();
    runAreaProductTests();
    runGapToleranceTests();

    std::printf("All EngineTests passed.\n");
    return 0;
}


/*// A not_evaluated check must never report passed=true (spec section 10:
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
*/