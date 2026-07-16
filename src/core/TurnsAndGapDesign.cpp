#include "TurnsAndGapDesign.h"
#include "GapDesign.h"
#include "TurnsCalculation.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr int kMaxIterations = 15;
constexpr double kGapStepMm = 0.01;     // practical gap constraint: machining resolution
constexpr double kMaxGapFraction = 0.4; // practical gap constraint: gap can't be a large fraction of the path length

// Seeds the initial turns estimate by reusing TurnsCalculation's existing
// N = round(sqrt(L/AL)) formula against the core's ungapped catalog AL,
// rather than duplicating that formula here (spec section 9: connect
// TurnsCalculation/GapDesign through an iteration module).
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

// precondition: core.aeMm2 > 0, core.leMm > 0, core.mu > 0,
// targetInductanceUH > 0
// postcondition: iterates turns and gap together until the integer turns
// count stabilizes (2-4 iterations typical for ferrite gap ranges), or
// returns converged=false with a rejection reason.
TurnsAndGapResult designTurnsAndGap(const CoreCandidate& core, double targetInductanceUH, double tolerancePercent) {
    TurnsAndGapResult result;

    double aeCm2 = core.aeMm2 / 100.0;
    double leCm = core.leMm / 10.0;
    double targetNh = targetInductanceUH * 1000.0;
    double maxGapMm = kMaxGapFraction * core.leMm;

    int turns = seedTurns(core, targetInductanceUH);

    for (int iteration = 0; iteration < kMaxIterations; ++iteration) {
        double gapCm = calculateRequiredGapCm(turns, aeCm2, leCm, core.mu, targetNh);
        double gapMm = std::max(0.0, gapCm * 10.0);
        gapMm = std::round(gapMm / kGapStepMm) * kGapStepMm;

        if (gapMm > maxGapMm) {
            result.converged = false;
            result.rejectionReasons.push_back(
                "required gap " + std::to_string(gapMm) + " mm exceeds the practical bound (" +
                std::to_string(maxGapMm) + " mm, 40% of the core's magnetic path length) for core '" +
                core.partNumber + "'");
            return result;
        }

        double alEff = calculateEffectiveAlNhPerTurnSq(aeCm2, leCm, core.mu, gapMm / 10.0);
        if (alEff <= 0.0) {
            result.converged = false;
            result.rejectionReasons.push_back("effective AL computed as non-positive for core '" +
                                               core.partNumber + "' - cannot design turns/gap");
            return result;
        }

        int newTurns = std::max(1, static_cast<int>(std::round(std::sqrt(targetNh / alEff))));

        if (newTurns == turns) {
            double actualNh = static_cast<double>(newTurns) * newTurns * alEff;
            double errorPercent = 100.0 * (actualNh - targetNh) / targetNh;

            result.turns = newTurns;
            result.gapMm = gapMm;
            result.effectiveAlNHPerTurnSquared = alEff;
            result.calculatedInductanceUH = actualNh / 1000.0;
            result.inductanceErrorPercent = errorPercent;
            result.withinTolerance = std::abs(errorPercent) <= tolerancePercent;
            result.converged = true;

            if (!result.withinTolerance) {
                result.rejectionReasons.push_back(
                    "calculated inductance " + std::to_string(result.calculatedInductanceUH) +
                    " uH is outside the " + std::to_string(tolerancePercent) + "% tolerance of the target " +
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
