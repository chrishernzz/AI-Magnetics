#pragma once

#include "core/sizing/CoreSelection.h"
#include "core/magnetics/GapDesign.h"

// STAGE 5: Turns
/*
Difference: no turns RATIO here — a buck inductor is a single
winding, not primary/secondary, since there's no isolation barrier.
Also, L is now an input (given), not something this stage derives.

*/

struct TurnsCalculationInput {
    double inductanceUH;
    //pass in the struct to get the 'AL'
    CoreSelectionResult core;
};

struct TurnsCalculationResult {
    int turns;
    double inductanceUH;
    double al;
};

TurnsCalculationResult calculateTurns(const TurnsCalculationInput& input);
