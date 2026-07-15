#include "TurnsCalculation.h"

//precondition: none
//postcondition: returns the number of turns required for the inductor based on the inductance and AL value of the core
TurnsCalculationResult calculateTurns(const TurnsCalculationInput& input) {
    //get the inductance 
    double inductanceH = input.inductanceUH * 1000;
    //Formula for turns: N = sqrt(L / AL)
    double turns = sqrt(inductanceH / input.core.al);

    return { static_cast<int>(std::round(turns)), input.inductanceUH, input.core.al };
}