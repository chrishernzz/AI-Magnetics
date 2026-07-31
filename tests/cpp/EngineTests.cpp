#include <cassert>
#include <cstdio>

//prototypes
void runGapDesignTests();
void runUnitConversionTests();
void runAreaProductTests();
void runGapToleranceTests();
void runPermeabilityRolloffTests();
void runValidationTests();
void runWindingDesignTests();
void runLossTests();
void runThermalTests();
void runRequirementDerivationServiceTests();
void runOperatingPointConfidenceServiceTests();
void runBottleneckAnalysisTests();
void runRankingExplanationTests();
void runRankingHighlightsTests();

int main() {
    //run all tests in sequence (in order). If every assert in every test passes, prints the final sucess line
    //if any assert anywhere fails, the process aborts at that point and the print is never printed out
    runGapDesignTests();
    runUnitConversionTests();
    runAreaProductTests();
    runGapToleranceTests();
    runPermeabilityRolloffTests();
    runValidationTests();
    runWindingDesignTests();
    runLossTests();
    runThermalTests();
    runRequirementDerivationServiceTests();
    runOperatingPointConfidenceServiceTests();
    runBottleneckAnalysisTests();
    runRankingExplanationTests();
    runRankingHighlightsTests();

    std::printf("All EngineTests passed.\n");
    return 0;
}