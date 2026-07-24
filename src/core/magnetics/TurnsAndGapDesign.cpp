#include "core/magnetics/TurnsAndGapDesign.h"
#include "core/magnetics/GapDesign.h"
#include "core/magnetics/TurnsCalculation.h"
#include "core/units/UnitConversions.h"
#include <algorithm>
#include <cmath>

namespace {
//this will be the maximum number of the turns-and-gap recalculation attempts before rejecting the design
constexpr int kMaxIterations = 15;

//precondition: Seeds the initial turns estimate by reusing TurnsCalculation's existing N = round(sqrt(L/AL)) formula against the core's ungapped catalog AL
//postcondition: returns the initial turns estimate for the given core and target inductance, rounded to the nearest integer and falls back to a minimum of 1 turn if none
int seedTurns(const CoreCandidate& core, double targetInductanceUH) {
    CoreSelectionResult seedCore;
    seedCore.partNumber = core.partNumber;
    seedCore.material = core.material;
    seedCore.mu = core.mu;
    seedCore.al = core.al;
    seedCore.ae = core.aeMm2;
    seedCore.wa = core.waMm2;
    seedCore.le = core.leMm;

    TurnsCalculationInput seedInput{targetInductanceUH, seedCore};
    TurnsCalculationResult seedResult = calculateTurns(seedInput);
    return std::max(1, seedResult.turns);
}

//precondition: turns > 0, gapMm >= 0
//postcondition: returns the inductance (uH) this core would produce at this turns count and gap - reused for
//the nominal result and both gap-tolerance sweep extremes so all three go through the exact same formula.
double inductanceAtGapUH(int turns, double gapMm, double aeCm2, double leCm, double muR) {
    double alEff = calculateEffectiveAlNhPerTurnSq(aeCm2, leCm, muR, units::mmToCm(gapMm));
    if (alEff <= 0.0) {
        return 0.0;
    }
    double actualNh = static_cast<double>(turns) * turns * alEff;
    return units::nHToUh(actualNh);
}
}  // namespace

//precondition: core.aeMm2 > 0, core.leMm > 0, core.mu > 0, targetInductanceUH > 0
//postcondition: iterates turns and gap together until the integer turns count stabilizes (2-4 iterations typical for ferrite gap ranges), then sweeps gap +-rules.gapTolerancePercent
//to check inductance stays within tolerancePercent at both extremes, or returns converged=false with a rejection reason.
TurnsAndGapResult designTurnsAndGap(const CoreCandidate& core, double targetInductanceUH, double tolerancePercent, const DesignRules& rules) {
    TurnsAndGapResult result;
    result.gapMethod = rules.gapMethod;

    //Only MachinedCenterLeg has a validated formula in Phase 1 (see GapMethod.h) - any other requested
    //method is rejected here rather than having the one validated formula silently applied to a technique
    //it was never checked against.
    if (rules.gapMethod != GapMethod::MachinedCenterLeg) {
        result.converged = false;
        result.rejectionReasons.push_back(
            "gap method is not implemented in Phase 1 (only MachinedCenterLeg has a validated formula)");
        return result;
    }

    //effective core area is converted from square millimeters to square centimeters (1cm2 = 100m2)
    double aeCm2 = units::mm2ToCm2(core.aeMm2);
    //magnetic path length is converted from millimeters to centimeters (1cm = 10mm)
    double leCm = units::mmToCm(core.leMm);
    //target inductance is converted from microhenries to nanohenries (1uH = 1000nH)
    double targetNh = units::uHToNh(targetInductanceUH);
    //this calculates the practical gap limit as a fraction of the core's magnetic path length
    double maxGapMm = rules.maxGapFraction * core.leMm;

    int turns = seedTurns(core, targetInductanceUH);

    //will run up to kMaxIterations times during each iteration it:
    /*
    Calculates the required gap for the current turns
    Rounds teh gap to a manufacturable value
    Calculates the effective AL with that gap
    Recalculates the required turns
    Checks whether the turns stopped changing
    */
    for (int iteration = 0; iteration < kMaxIterations; ++iteration) {
        //given the core, number of turns, and target inductance, what air gap is required?
        double gapCm = calculateRequiredGapCm(turns, aeCm2, leCm, core.mu, targetNh);
        //convert centimeters to millimeters to prevent a negative gap
        double gapMm = std::max(0.0, units::cmToMm(gapCm));
        gapMm = std::round(gapMm / rules.gapStepMm) * rules.gapStepMm;

        //check whether the required gap exceeds the practical limit. If it does, return a rejection reason and set converged=false.
        if (gapMm > maxGapMm) {
            result.converged = false;
            result.rejectionReasons.push_back("required gap " + std::to_string(gapMm) + " mm exceeds the practical bound (" + std::to_string(maxGapMm) + " mm, " +
                std::to_string(rules.maxGapFraction * 100.0) + "% of the core's magnetic path length) for core '" + core.partNumber + "'");
            return result;
        }

        double alEff = calculateEffectiveAlNhPerTurnSq(aeCm2, leCm, core.mu, units::mmToCm(gapMm));
        //if zero or negative effective AL is calculated, return a rejection reason and set converged=false.
        if (alEff <= 0.0) {
            result.converged = false;
            result.rejectionReasons.push_back("effective AL computed as non-positive for core '" + core.partNumber + "' - cannot design turns/gap");
            return result;
        }

        //recalculate the number of turns using N = sqrt(L/ AL)
        int newTurns = std::max(1, static_cast<int>(std::round(std::sqrt(targetNh / alEff))));

        //convergence check here - After calculating the gap and effective AL, did we get the same number of turns we started this iteration with, if yes then the design has stablized
        if (newTurns == turns) {
            //calculate the actual inductance that uses L = N2 x AL
            double actualNh = static_cast<double>(newTurns) * newTurns * alEff;
            //calculate the error percentage between the actual inductance and the target inductance (postive error means the actual inductance is above the target, vice versa for negative)
            double errorPercent = 100.0 * (actualNh - targetNh) / targetNh;

            result.turns = newTurns;
            result.gapMm = gapMm;
            result.effectiveAlNHPerTurnSquared = alEff;
            result.calculatedInductanceUH = units::nHToUh(actualNh);
            result.inductanceErrorPercent = errorPercent;
            result.withinTolerance = std::abs(errorPercent) <= tolerancePercent;
            result.converged = true;

            if (!result.withinTolerance) {
                result.rejectionReasons.push_back("calculated inductance " + std::to_string(result.calculatedInductanceUH) + " uH is outside the " + std::to_string(tolerancePercent) + "% tolerance of the target " +
                    std::to_string(targetInductanceUH) + " uH (error " + std::to_string(errorPercent) + "%)");
            }

            //Gap-tolerance sweep: a nominal gap that lands within tolerance can still fail once realistic
            //mechanical tolerance is accounted for - checked at both extremes, turns held fixed.
            result.gapMinMm = std::max(0.0, gapMm * (1.0 - rules.gapTolerancePercent / 100.0));
            result.gapMaxMm = gapMm * (1.0 + rules.gapTolerancePercent / 100.0);
            result.inductanceAtMinGapUH = inductanceAtGapUH(newTurns, result.gapMinMm, aeCm2, leCm, core.mu);
            result.inductanceAtMaxGapUH = inductanceAtGapUH(newTurns, result.gapMaxMm, aeCm2, leCm, core.mu);

            double minGapErrorPercent = 100.0 * (units::uHToNh(result.inductanceAtMinGapUH) - targetNh) / targetNh;
            double maxGapErrorPercent = 100.0 * (units::uHToNh(result.inductanceAtMaxGapUH) - targetNh) / targetNh;
            bool minGapWithinTolerance = std::abs(minGapErrorPercent) <= tolerancePercent;
            bool maxGapWithinTolerance = std::abs(maxGapErrorPercent) <= tolerancePercent;
            result.inductanceWithinToleranceAcrossGapRange = minGapWithinTolerance && maxGapWithinTolerance;

            if (!result.inductanceWithinToleranceAcrossGapRange) {
                std::string whichExtreme = !minGapWithinTolerance && !maxGapWithinTolerance ? "both the min and max"
                                            : !minGapWithinTolerance                        ? "the min"
                                                                                             : "the max";
                result.rejectionReasons.push_back("gap tolerance +-" + std::to_string(rules.gapTolerancePercent) +
                    "% pushes calculated inductance outside the requested tolerance at " + whichExtreme + " gap extreme (" +
                    std::to_string(result.gapMinMm) + " mm to " + std::to_string(result.gapMaxMm) + " mm)");
            }

            //Small-gap manufacturability warning - a caveat, not a rejection, since the design is still
            //physically valid, just harder to reliably machine/lap by hand.
            if (gapMm > 0.0 && gapMm < rules.minManufacturableGapMm) {
                result.smallGapWarning = true;
                result.smallGapWarningReason = "calculated gap " + std::to_string(gapMm) + " mm is below the Phase 1 minimum-manufacturable-gap estimate (" +
                    std::to_string(rules.minManufacturableGapMm) + " mm) - a machined/lapped gap this small may not be reliably reproducible";
            }

            return result;
        }

        turns = newTurns;
    }

    result.converged = false;
    result.rejectionReasons.push_back("turns/gap did not converge within " + std::to_string(kMaxIterations) +
                                       " iterations for core '" + core.partNumber + "'");
    return result;
}
