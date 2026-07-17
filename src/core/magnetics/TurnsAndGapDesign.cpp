#include "core/magnetics/TurnsAndGapDesign.h"
#include "core/magnetics/GapDesign.h"
#include "core/magnetics/TurnsCalculation.h"
#include <algorithm>
#include <cmath>

namespace {
//this will be the maximum number of the turns-and-gap recalculation attempts before rejecting the design
constexpr int kMaxIterations = 15;
//practical gap constraint: machining resolution - this means the gap can only be specified in increments of 0.01mm
constexpr double kGapStepMm = 0.01;    
//practical gap constraint: gap cannot be greater than 40% of the core's magnetic path length
constexpr double kMaxGapFraction = 0.4;

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
}  // namespace

//precondition: core.aeMm2 > 0, core.leMm > 0, core.mu > 0, targetInductanceUH > 0
//postcondition: iterates turns and gap together until the integer turns count stabilizes (2-4 iterations typical for ferrite gap ranges), or returns converged=false with a rejection reason.
TurnsAndGapResult designTurnsAndGap(const CoreCandidate& core, double targetInductanceUH, double tolerancePercent) {
    TurnsAndGapResult result;

    //effective core area is converted from square millimeters to square centimeters (1cm2 = 100m2)
    double aeCm2 = core.aeMm2 / 100.0;
    //magnetic path length is converted from millimeters to centimeters (1cm = 10mm)
    double leCm = core.leMm / 10.0;
    //target inductance is converted from microhenries to nanohenries (1uH = 1000nH)
    double targetNh = targetInductanceUH * 1000.0;
    //this calculates the 40% practical gap limit 
    double maxGapMm = kMaxGapFraction * core.leMm;


    int turns = seedTurns(core, targetInductanceUH);

    //will run up to 15 times during each iteration it:
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
        double gapMm = std::max(0.0, gapCm * 10.0);
        gapMm = std::round(gapMm / kGapStepMm) * kGapStepMm;

        //check whether the required gap exceeds the practical limit of 40% of the core's magnetic path length. If it does, return a rejection reason and set converged=false.
        if (gapMm > maxGapMm) {
            result.converged = false;
            result.rejectionReasons.push_back("required gap " + std::to_string(gapMm) + " mm exceeds the practical bound (" + std::to_string(maxGapMm) + " mm, 40% of the core's magnetic path length) for core '" + core.partNumber + "'");
            return result;
        }

        double alEff = calculateEffectiveAlNhPerTurnSq(aeCm2, leCm, core.mu, gapMm / 10.0);
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
            result.calculatedInductanceUH = actualNh / 1000.0;
            result.inductanceErrorPercent = errorPercent;
            result.withinTolerance = std::abs(errorPercent) <= tolerancePercent;
            result.converged = true;

            if (!result.withinTolerance) {
                result.rejectionReasons.push_back("calculated inductance " + std::to_string(result.calculatedInductanceUH) + " uH is outside the " + std::to_string(tolerancePercent) + "% tolerance of the target " +
                    std::to_string(targetInductanceUH) + " uH (error " + std::to_string(errorPercent) + "%)");
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
