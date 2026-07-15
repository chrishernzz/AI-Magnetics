#include "TurnsCalculation.h"
#include <cmath>
//precondition: none
//postcondition: returns the number of turns required for the inductor based on the inductance and AL value of the core
TurnsCalculationResult calculateTurns(const TurnsCalculationInput& input) {
    //get the inductance 
    double inductanceNH = input.inductanceUH * 1000;
    //Formula for turns: N = sqrt(L / AL)
    double turns = sqrt(inductanceNH / input.core.al);

    return { static_cast<int>(std::round(turns)), input.inductanceUH, input.core.al };
}