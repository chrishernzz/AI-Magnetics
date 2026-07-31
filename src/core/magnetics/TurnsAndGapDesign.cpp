#include "core/magnetics/TurnsAndGapDesign.h"
#include "core/magnetics/GapDesign.h"
#include "core/magnetics/PermeabilityRolloff.h"
#include "core/magnetics/TurnsCalculation.h"
#include "core/sizing/FluxLimit.h"
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

    //call the function to calculate the turns and it will have the formula 
    TurnsCalculationInput seedInput{targetInductanceUH, seedCore};
    TurnsCalculationResult seedResult = calculateTurns(seedInput);
    return std::max(1, seedResult.turns);
}

//precondition: targetInductanceUH > 0, peakCurrentA > 0, core.aeMm2 > 0, appliedFluxLimitT > 0
//postcondition: returns the minimum integer turns count N such that Bpk = L*Ipk/(N*Ae) is at or below
//the margin-derated flux limit (appliedFluxLimitT * (1 - rules.minimumSaturationMarginPercent/100)) - i.e.
//a turns count that, if realized, satisfies SaturationValidation's margin check against the SAME limit
//PeakFluxValidation/SaturationValidation apply downstream (see applicableFluxLimit in FluxLimit.h), not a
//separate, potentially-inconsistent threshold. Rounds up (ceil), never down, so integer rounding never
//leaves Bpk fractionally above the derated limit. Returns 0 (a sentinel, never a real turns count) if the
//derated limit is non-positive - callers must treat 0 as "no flux-aware floor available."
int minimumTurnsForSaturationMargin(const CoreCandidate& core, double targetInductanceUH, double peakCurrentA,
                                     double appliedFluxLimitT, const DesignRules& rules) {
    double deratedLimitT = appliedFluxLimitT * (1.0 - rules.minimumSaturationMarginPercent / 100.0);
    if (deratedLimitT <= 0.0) {
        return 0;
    }
    double inductanceH = units::uHToH(targetInductanceUH);
    double aeM2 = units::mm2ToM2(core.aeMm2);
    double nMinRaw = (inductanceH * peakCurrentA) / (aeM2 * deratedLimitT);
    return std::max(1, static_cast<int>(std::ceil(nMinRaw)));
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
TurnsAndGapResult designTurnsAndGap(const CoreCandidate& core, const MaterialCandidate& material,
                                     double targetInductanceUH, double tolerancePercent,
                                     const std::optional<double>& peakCurrentA, double rmsCurrentA, const DesignRules& rules) {
    TurnsAndGapResult result;

    //Real powder toroid materials (MPP/Kool Mu/High Flux/Sendust, etc.) achieve their working permeability
    //through gapping distributed at the powder-particle level, baked into the catalog AL - there is no
    //discrete machined air gap to report. Shape alone ("Toroid") is NOT a sufficient signal for this - the
    //catalog also carries plenty of real ferrite toroids (e.g. N87, T35, T65, and Fair-Rite's 67/77/79/80
    //grades), which get their permeability from the ferrite chemistry itself, not particle-level gapping,
    //and need the exact same machined-gap formula as a two-piece core - a real user report (a passing N87
    //toroid candidate reporting gapMm=0.0/Distributed) caught this. real_cores.csv's own MaterialType column
    //("ferrite"/"powder", sourced directly from PyOpenMagnetics' material.material field - see
    //scripts/export_real_data.py) is the real signal: only a Toroid whose material is confirmed "powder" is
    //distributed-gap. An empty/unknown materialType is treated conservatively - it runs the real formula
    //below rather than being assumed powder.
    bool isDistributedGapCore = core.coreShape == "Toroid" && core.materialType == "powder";
    result.gapMethod = isDistributedGapCore ? GapMethod::Distributed : rules.gapMethod;

    //Only MachinedCenterLeg has a validated formula in Phase 1 (see GapMethod.h) - any other requested
    //method is rejected here rather than having the one validated formula silently applied to a technique
    //it was never checked against. Distributed-gap toroids are exempt from this gate since they never run
    //the discrete-gap formula at all - see below.
    if (!isDistributedGapCore && rules.gapMethod != GapMethod::MachinedCenterLeg) {
        result.converged = false;
        result.rejectionReasons.push_back("gap method is not implemented in Phase 1 (only MachinedCenterLeg has a validated formula)");
        return result;
    }

    //target inductance is converted from microhenries to nanohenries (1uH = 1000nH)
    double targetNh = units::uHToNh(targetInductanceUH);

    //Distributed-gap toroids: no center leg exists to machine a discrete gap into, so the McLyman
    //required-gap iteration below (which solves for an ADDITIONAL air gap on top of the ungapped AL) does
    //not apply - the only lever available is turns. Reporting a nonzero "gap" here would imply a machinable
    //dimension that does not physically exist on this part. No gap-tolerance sweep either - there is no gap
    //dimension for mechanical tolerance to act on; AL manufacturing tolerance is a different, real concern
    //this Phase 1 dataset does not carry data for (see DATA_FILES.md).
    //
    //core.al is the material's INITIAL permeability (measured at ~0 A DC bias), not a constant - real
    //powder cores roll off permeability as DC current rises (see PermeabilityRolloff.h; a real user report
    //caught this: the old code solved N=sqrt(L/AL0) once and reported the SAME inductance at 0A and at the
    //design's real operating current, when the true inductance at current is measurably lower). When a real
    //manufacturer DC-bias curve exists for this material AND a real operating current is known, turns and
    //the rolled-off AL are solved together below as a fixed point: turns -> H -> %mu(H) -> AL_eff -> turns
    //required against AL_eff -> repeat, capped at kMaxIterations like the ferrite gap loop below. This
    //iteration is monotonically increasing in turns (more turns raises H, which lowers %mu, which demands
    //still more turns) - a genuine "no turns count on this core achieves the target L at this current"
    //case (operating near the material's real saturation current) fails to converge and is rejected with a
    //real reason, not forced to a number. When no curve exists yet for this material, or no current is
    //known at all, this falls back to the original AL0-only formula (result.usesDCBiasRolloffCurve stays
    //false either way, so callers can tell the difference from a real 0%-bias result).
    if (isDistributedGapCore) {
        int turns = std::max(1, seedTurns(core, targetInductanceUH));

        DCBiasCurveLookup rolloffCurve = findDCBiasCurve(core.material);

        std::optional<double> biasCurrentA;
        bool biasCurrentIsRmsFloor = false;
        if (peakCurrentA.has_value() && *peakCurrentA > 0.0) {
            biasCurrentA = peakCurrentA;
        } else if (rmsCurrentA > 0.0) {
            biasCurrentA = rmsCurrentA;
            biasCurrentIsRmsFloor = true;
        }

        double alEffNh = core.al;
        double appliedHOe = 0.0;
        double appliedPercentMu = 100.0;
        bool rolloffApplied = false;

        if (rolloffCurve.found && biasCurrentA.has_value()) {
            int previousTurns = -1;
            bool stabilized = false;
            for (int i = 0; i < kMaxIterations; ++i) {
                double hOe = dcMagnetizingForceOe(turns, *biasCurrentA, core.leMm);
                double percentMu = percentInitialPermeability(hOe, rolloffCurve.curve);
                double trialAlEffNh = core.al * (percentMu / 100.0);
                if (trialAlEffNh <= 0.0) {
                    break;
                }
                int requiredTurns = std::max(1, static_cast<int>(std::round(std::sqrt(targetNh / trialAlEffNh))));

                appliedHOe = hOe;
                appliedPercentMu = percentMu;
                alEffNh = trialAlEffNh;

                if (requiredTurns == turns) {
                    stabilized = true;
                    break;
                }
                if (requiredTurns == previousTurns) {
                    //rounding 2-cycle between two adjacent integer turns counts near the true (fractional)
                    //fixed point - resolve conservatively (more turns -> lower flux, never the reverse).
                    turns = std::max(turns, requiredTurns);
                    stabilized = true;
                    break;
                }
                previousTurns = turns;
                turns = requiredTurns;
            }

            if (!stabilized) {
                result.converged = false;
                result.rejectionReasons.push_back("no turns count converged for this distributed-gap core at the real "
                    "operating current (" + std::to_string(*biasCurrentA) + " A" +
                    (biasCurrentIsRmsFloor ? ", RMS used as a guaranteed lower bound on unsupplied peak current" : "") +
                    ") within " + std::to_string(kMaxIterations) + " iterations - DC-bias permeability roll-off means "
                    "more turns raises the magnetizing force further, which rolls off permeability further; this core's "
                    "real saturation behavior may not support the target inductance at this current at all");
                return result;
            }
            rolloffApplied = true;
        }

        double actualNh = static_cast<double>(turns) * turns * alEffNh;
        double errorPercent = 100.0 * (actualNh - targetNh) / targetNh;

        result.turns = turns;
        result.gapMm = 0.0;
        result.effectiveAlNHPerTurnSquared = alEffNh;
        result.calculatedInductanceUH = units::nHToUh(actualNh);
        result.inductanceErrorPercent = errorPercent;
        result.withinTolerance = std::abs(errorPercent) <= tolerancePercent;
        result.converged = true;
        result.gapMinMm = 0.0;
        result.gapMaxMm = 0.0;
        result.inductanceAtMinGapUH = result.calculatedInductanceUH;
        result.inductanceAtMaxGapUH = result.calculatedInductanceUH;
        result.inductanceWithinToleranceAcrossGapRange = result.withinTolerance;
        result.usesDCBiasRolloffCurve = rolloffApplied;
        result.dcMagnetizingForceOe = rolloffApplied ? appliedHOe : 0.0;
        result.percentInitialPermeabilityAtOperatingCurrent = rolloffApplied ? appliedPercentMu : 100.0;
        result.dcBiasRolloffUsedRmsFloor = rolloffApplied && biasCurrentIsRmsFloor;

        if (!result.withinTolerance) {
            result.rejectionReasons.push_back("calculated inductance " + std::to_string(result.calculatedInductanceUH) +
                " uH (turns=" + std::to_string(turns) + " against " + (rolloffApplied ? "DC-bias-rolled-off AL " : "catalog AL ") +
                std::to_string(alEffNh) + " nH/turn^2" + (rolloffApplied ? " (" + std::to_string(appliedPercentMu) +
                "% of initial permeability at " + std::to_string(appliedHOe) + " Oe)" : "") +
                ", distributed-gap core, no machined gap available to fine-tune) is outside the " +
                std::to_string(tolerancePercent) + "% tolerance of the target " + std::to_string(targetInductanceUH) +
                " uH (error " + std::to_string(errorPercent) + "%)");
        }
        return result;
    }

    //effective core area is converted from square millimeters to square centimeters (1cm2 = 100m2)
    double aeCm2 = units::mm2ToCm2(core.aeMm2);
    //magnetic path length is converted from millimeters to centimeters (1cm = 10mm)
    double leCm = units::mmToCm(core.leMm);
    //this calculates the practical gap limit as a fraction of the core's magnetic path length
    double maxGapMm = rules.maxGapFraction * core.leMm;

    int turns = seedTurns(core, targetInductanceUH);

    //Flux-aware turns floor: the plain inductance-matching seed above is the MINIMUM possible turns count
    //for this target L (it comes from the core's ungapped/maximum AL) - it has no awareness of peak current
    //or flux density, so on many real ferrite cores it converges directly to a zero-gap, minimum-turns
    //design that PeakFluxValidation/SaturationValidation then reject downstream with no retry (a real user
    //report: E100/60/28-3C90 at 3000uH/5A peak converged at turns=20/gapMm=0.0, Bpk=1.03T vs Bmax=0.47T,
    //when a real ~49-turn/~0.6mm-gap design exists on the SAME core and passes). When a real peak current is
    //known, raise the starting turns to whatever this exact core/target/limit combination requires to
    //already respect rules.minimumSaturationMarginPercent - Bpk = L*Ipk/(N*Ae) inverted for N. This only
    //ever RAISES the seed (max(), never lowered) - a core where the inductance-matching seed already had
    //enough margin is completely unaffected.
    //
    //When peakCurrentA is absent but rmsCurrentA > 0.0, the same seed runs against rmsCurrentA instead - a
    //mathematically guaranteed LOWER BOUND on the real (unsupplied) peak current (RMS <= peak always, for
    //any unidirectional inductor-current waveform), never a substitute for the real peak. Without this, an
    //RMS-only request has NO flux-aware floor at all, and the plain inductance-matching seed can converge on
    //a minimum-turns, ultra-high-permeability ferrite design that saturates far below the stated RMS current
    //(a real user report: 3000uH/5A RMS with no peak supplied ranked a mu=5654 26-turn ungapped toroid as
    //its #1 passing candidate - that design saturates at ~0.12A, ~40x below the request's own 5A RMS value).
    //Using the RMS floor here does not confirm the design is safe (real ripple could still push the true
    //peak, and therefore true required margin, higher) - PeakFluxValidation/SaturationValidation's own
    //RMS-floor certain-failure check (see DesignValidation.cpp) is what actually rejects a design that's
    //still broken even at this floor; this seed just stops the solver from handing that design a trivial
    //minimum-turns win in the first place.
    std::optional<double> fluxAwareSeedCurrentA;
    bool seedIsRmsFloor = false;
    if (peakCurrentA.has_value() && *peakCurrentA > 0.0) {
        fluxAwareSeedCurrentA = peakCurrentA;
    } else if (rmsCurrentA > 0.0) {
        fluxAwareSeedCurrentA = rmsCurrentA;
        seedIsRmsFloor = true;
    }

    if (fluxAwareSeedCurrentA.has_value()) {
        FluxLimit limit = applicableFluxLimit(material, rules);
        int fluxAwareMinTurns = minimumTurnsForSaturationMargin(core, targetInductanceUH, *fluxAwareSeedCurrentA, limit.limitT, rules);
        if (fluxAwareMinTurns > turns) {
            result.turnsRaisedForSaturationMargin = true;
            result.turnsRaisedUsingRmsFloor = seedIsRmsFloor;
            result.turnsRaisedForSaturationMarginReason =
                "seed turns raised from " + std::to_string(turns) + " (inductance-matching minimum, ungapped AL) to " +
                std::to_string(fluxAwareMinTurns) + " turns to respect the " +
                std::to_string(rules.minimumSaturationMarginPercent) + "% saturation margin at " +
                (seedIsRmsFloor ? "RMS current (used as a guaranteed lower bound on peak current, since no real peak current was supplied) "
                                 : "peak current ") +
                std::to_string(*fluxAwareSeedCurrentA) + " A against the applicable " + std::to_string(limit.limitT) +
                " T flux limit (" + (limit.usedDefault ? "Phase 1 default" : "material-specific") +
                ") - PeakFluxValidation/SaturationValidation still verify this independently below" +
                (seedIsRmsFloor ? "; the real (unsupplied) peak current could still require more margin than this floor guarantees" : "");
            turns = fluxAwareMinTurns;
        }
    }

    //will run up to kMaxIterations times during each iteration it:
    //Calculates the required gap for the current turns
    //Rounds the gap to a manufacturable value
    //Calculates the effective AL with that gap
    //Recalculates the required turns
    //Checks whether the turns stopped changing
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
    result.rejectionReasons.push_back("turns/gap did not converge within " + std::to_string(kMaxIterations) + " iterations for core '" + core.partNumber + "'");
    return result;
}
