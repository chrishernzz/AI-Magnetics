#pragma once
#include <optional>
#include <string>
#include <vector>
#include "core/sizing/CoreEvaluation.h"
#include "core/sizing/MaterialEvaluation.h"
#include "core/magnetics/GapMethod.h"
#include "rules/DesignRules.h"

/*
STAGE 5: Turns and air-gap design
Turns and gap are connected design variables (spec section 9): gap
depends on turns, and effective AL (and therefore turns) depends on gap.
This module iterates the two together instead of letting
TurnsCalculation and GapDesign produce independent final answers -
TurnsCalculation::calculateTurns() is reused as the seed-turns estimator
(ungapped AL), then GapDesign's formulas refine turns and gap together
until the integer turns count stabilizes.

Once turns/gap converge, the gap is swept +-DesignRules.gapTolerancePercent
(a documented mechanical-tolerance estimate, not real per-core
manufacturing data) to check whether the resulting inductance still
falls within the requested tolerance at both extremes - a design whose
nominal gap looks fine but whose realistic manufacturing tolerance
pushes inductance out of spec is flagged, not silently accepted.

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

    //which gapping technique this result assumes - see GapMethod.h. Only MachinedCenterLeg has a validated
    //formula in Phase 1; any other value in rules.gapMethod is rejected before this struct's other fields
    //are populated (converged=false with a clear reason).
    GapMethod gapMethod = GapMethod::MachinedCenterLeg;

    //nominal gap swept by +-rules.gapTolerancePercent, turns held fixed at the converged value.
    double gapMinMm = 0.0;
    double gapMaxMm = 0.0;
    double inductanceAtMinGapUH = 0.0;
    double inductanceAtMaxGapUH = 0.0;
    //true only if BOTH gap extremes keep calculated inductance within tolerancePercent of the target -
    //a nominal gap that looks fine but whose realistic manufacturing tolerance pushes inductance out of
    //spec at either extreme sets this false, with a rejection reason explaining which extreme failed.
    bool inductanceWithinToleranceAcrossGapRange = false;

    //true when 0 < gapMm < rules.minManufacturableGapMm - a warning (design still proceeds), not a
    //rejection, since it's a manufacturability caveat rather than a physical impossibility.
    bool smallGapWarning = false;
    std::string smallGapWarningReason;

    //true when peakCurrentA was supplied and the flux-aware seed (minimumTurnsForSaturationMargin, see .cpp)
    //exceeded the plain inductance-matching seed turns, raising the starting turns count specifically to
    //respect rules.minimumSaturationMarginPercent before iterating turns/gap. Always false when peakCurrentA
    //is absent (that flow is completely unchanged - see designTurnsAndGap()'s peakCurrentA parameter).
    //Never a promise that the resulting design actually passes PeakFluxValidation/SaturationValidation -
    //those still run independently against whatever (turns, gap) the solver converges to.
    bool turnsRaisedForSaturationMargin = false;
    std::string turnsRaisedForSaturationMarginReason;
};

// precondition: core.aeMm2 > 0, core.leMm > 0, core.mu > 0, targetInductanceUH > 0
// postcondition: returns turns/gap that realize targetInductanceUH on this core within tolerancePercent,
// or converged=false with rejectionReasons explaining why. When peakCurrentA is supplied, the starting
// turns count is raised (never lowered) above the plain inductance-matching minimum when needed to
// respect rules.minimumSaturationMarginPercent - see TurnsAndGapResult::turnsRaisedForSaturationMargin.
// When peakCurrentA is std::nullopt, behavior is byte-for-byte identical to before this parameter existed.
TurnsAndGapResult designTurnsAndGap(const CoreCandidate& core, const MaterialCandidate& material,
                                     double targetInductanceUH, double tolerancePercent,
                                     const std::optional<double>& peakCurrentA, const DesignRules& rules);
