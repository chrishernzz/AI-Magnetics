#pragma once
#include <string>
#include <vector>
#include "core/sizing/CoreEvaluation.h"

/*
STAGE 5: Turns and air-gap design
Turns and gap are connected design variables (spec section 9): gap
depends on turns, and effective AL (and therefore turns) depends on gap.
This module iterates the two together instead of letting
TurnsCalculation and GapDesign produce independent final answers -
TurnsCalculation::calculateTurns() is reused as the seed-turns estimator
(ungapped AL), then GapDesign's formulas refine turns and gap together
until the integer turns count stabilizes.

*/
struct TurnsAndGapResult {
    int turns = 0;
    double gapMm = 0.0;
    double effectiveAlNHPerTurnSquared = 0.0;
    double calculatedInductanceUH = 0.0;
    double inductanceErrorPercent = 0.0;
    bool withinTolerance = false;
    bool converged = false;
    std::vector<std::string> rejectionReasons;
};

// precondition: core.aeMm2 > 0, core.leMm > 0, core.mu > 0,
// targetInductanceUH > 0
// postcondition: returns turns/gap that realize targetInductanceUH on this
// core within tolerancePercent, or converged=false with rejectionReasons
// explaining why (gap exceeds a practical bound, or no stable turns count
// found within the iteration cap).
TurnsAndGapResult designTurnsAndGap(const CoreCandidate& core, double targetInductanceUH, double tolerancePercent);
