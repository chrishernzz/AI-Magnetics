#include <cassert>
#include <cstdio>
#include "TestHelpers.h"
#include "core/sizing/AreaProduct.h"

namespace {

//precondition: none
//postcondition: returns the expected stored energy for a given inductance and peak current, confirming the formula is implemented correctly. If the assert fails, the process aborts here and no later test runs
void testCalculateStoredEnergy() {
    //1 uH
    double inductanceH = 1e-6; 
    //10 A
    double peakCurrentA = 10.0; 

    //real answer 
    double expectedEnergy = 0.5 * inductanceH * peakCurrentA * peakCurrentA;
    //what my function does
    double actualEnergy = calculateStoredEnergy(inductanceH, peakCurrentA);

    assert(approxEqual(actualEnergy, expectedEnergy, 1e-12));
    std::printf("testCalculateStoredEnergy: ok\n");
}

//precondition: input has real positive values for inductanceH, peakCurrentA, windowUtilization, fluxDensityT, and currentDensityAPerCm2
//postconditin: calculateAp must return Ap = (2 * Emax * 1e4) / (Ku * Bmax * J), where Emax = 0.5 * L * Ipk^2, and the other variables are taken from the input struct. If the assert fails, the process aborts here and no later test runs
void testAreaProduct() {
    //start with all 0s 
    AreaProductInput input {};
    input.inductanceH = 1e-6;
    input.peakCurrentA = 10.0;
    input.windowUtilization = 0.4;
    input.fluxDensityT = 0.3;
    input.currentDensityAPerCm2 = 400.0;

    //(2 * 0.5 * 1e-6 * 10^2 * 1e4) / (0.4 * 0.3 * 400)
    double expectedAp = 0.02083333333333333; 
    double actualAp = calculateAp(input);
    assert(approxEqual(actualAp, expectedAp, 1e-9));
    std::printf("testCalculateAp: Ap=%.10f (expected ~%.10f)\n", actualAp, expectedAp);
}

} // namespace 


void runAreaProductTests() {
    testCalculateStoredEnergy();
    testAreaProduct();
    std::printf("All AreaProductTests passed.\n");
}
