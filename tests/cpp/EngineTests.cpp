#include <cassert>
#include <cstdio>

//prototypes
void runGapDesignTests();
void runUnitConversionTests();
void runAreaProductTests();
void runGapToleranceTests();
void runValidationTests();
void runWindingDesignTests();

int main() {
    //run all tests in sequence (in order). If every assert in every test passes, prints the final sucess line
    //if any assert anywhere fails, the process aborts at that point and the print is never printed out
    runGapDesignTests();
    runUnitConversionTests();
    runAreaProductTests();
    runGapToleranceTests();
    runValidationTests();
    runWindingDesignTests();

    std::printf("All EngineTests passed.\n");
    return 0;
}