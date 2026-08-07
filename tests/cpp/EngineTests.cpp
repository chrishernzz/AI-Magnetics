#include <cassert>
#include <cstdio>

//prototypes
void runGapDesignTests();
void runUnitConversionTests();
void runAreaProductTests();
void runCoreEvaluationTests();
void runGapToleranceTests();
void runPermeabilityRolloffTests();
void runValidationTests();
void runWindingDesignTests();
void runLossTests();
void runThermalTests();
void runRequirementDerivationServiceTests();
void runRankingHighlightsTests();

int main() {
    //run all tests in sequence (in order). If every assert in every test passes, prints the final sucess line
    //if any assert anywhere fails, the process aborts at that point and the print is never printed out
    runGapDesignTests();
    runUnitConversionTests();
    runAreaProductTests();
    runCoreEvaluationTests();
    runGapToleranceTests();
    runPermeabilityRolloffTests();
    runValidationTests();
    runWindingDesignTests();
    runLossTests();
    runThermalTests();
    runRequirementDerivationServiceTests();
    runRankingHighlightsTests();

    std::printf("All EngineTests passed.\n");
    return 0;
}