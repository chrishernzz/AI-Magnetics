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

//no real core's winding window could ever fit anywhere near this many turns - a real physical bound, not an arbitrary cap. Guards every N = sqrt(target/AL) computation below: when effective AL has rolled off (or
//been gapped) to something very small but not exactly zero/negative, targetNh/AL can be astronomically large, and casting sqrt() of that to int is undefined behavior - observed in practice as silently
//returning INT_MAX (2147483647), which then posed as a real "converged" turns count downstream instead of a rejection. Checking the ratio against this bound before the sqrt/cast avoids ever exercising that UB.
constexpr double kMaxTurnsSquared = 1e12;  //(1,000,000 turns)^2

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
//postcondition: returns the minimum integer turns count N such that Bpk = L*Ipk/(N*Ae) is at or below the margin-derated flux limit (appliedFluxLimitT * (1 - rules.minimumSaturationMarginPercent/100)) - i.e.
//a turns count that, if realized, satisfies SaturationValidation's margin check against the SAME limit PeakFluxValidation/SaturationValidation apply downstream (see applicableFluxLimit in FluxLimit.h), not a
//separate, potentially-inconsistent threshold. Rounds up (ceil), never down, so integer rounding never leaves Bpk fractionally above the derated limit. Returns 0 (a sentinel, never a real turns count) if the
//derated limit is non-positive - callers must treat 0 as "no flux-aware floor available."
int minimumTurnsForSaturationMargin(const CoreCandidate& core, double targetInductanceUH, double peakCurrentA, double appliedFluxLimitT, const DesignRules& rules) {
    double deratedLimitT = appliedFluxLimitT * (1.0 - rules.minimumSaturationMarginPercent / 100.0);
    if (deratedLimitT <= 0.0) {
        return 0;
    }
    double inductanceH = units::uHToH(targetInductanceUH);
    double aeM2 = units::mm2ToM2(core.aeMm2);
    double nMinRaw = (inductanceH * peakCurrentA) / (aeM2 * deratedLimitT);
    if (!std::isfinite(nMinRaw) || nMinRaw > kMaxTurnsSquared) {
        //same "0 = no flux-aware floor available" sentinel documented above - a derated limit small enough to demand this many turns isn't a real floor a caller should act on.
        return 0;
    }
    return std::max(1, static_cast<int>(std::ceil(nMinRaw)));
}

//precondition: turns > 0, gapMm >= 0
//postcondition: returns the inductance (uH) this core would produce at this turns count and gap - reused for the nominal result and both gap-tolerance sweep extremes so all three go through the exact same formula.
double inductanceAtGapUH(int turns, double gapMm, double aeCm2, double leCm, double muR) {
    double alEff = calculateEffectiveAlNhPerTurnSq(aeCm2, leCm, muR, units::mmToCm(gapMm));
    if (alEff <= 0.0) {
        return 0.0;
    }
    double actualNh = static_cast<double>(turns) * turns * alEff;
    return units::nHToUh(actualNh);
}
}  //namespace

//Distributed Gap Core
TurnsAndGapResult solveDistributedGapCore(double targetNh, const CoreCandidate& core, double targetInductanceUH, double tolerancePercent, const std::optional<double>& peakCurrentA, double rmsCurrentA) {
    TurnsAndGapResult result;
    //C055439A2: turns must be solved ONCE from the catalog's zero-bias AL0 and then held fixed - DC-bias roll-off degrades the inductance a fixed winding
    //delivers at real operating current, it does not change how many turns are physically on the core. The previous behavior here iterated turns and the rolled-off AL together as a fixed point
    //(turns -> H -> %mu(H) -> AL_eff -> turns required against AL_eff -> repeat) so the CALCULATED inductance still hit target AT the real operating current - physically real, but for a
    //winding-fit/copper-loss/thermal-evaluation purpose: a real magnetics engineer reviewing this tool's output expects the turns count to be the zero-bias design value (e.g. 149
    //for C055439A2), with DC-bias roll-off reported as a separate, informational inductance-at-current number (e.g. 1.5mH at 5A) - not folded back into a compensated, larger winding (e.g. 289 turns)
    //that no longer represents what the tool says it's recommending building.
    int turns = std::max(1, seedTurns(core, targetInductanceUH));

    //this check is a sanity/overflow gaurd that rejects designs where the core AL value is so small that the required turns count would become physically unrealistic
    if (core.al <= 0.0 || targetNh / core.al > kMaxTurnsSquared) {
        //Same physical-safety guard the ferrite path uses below (see kMaxTurnsSquared's own comment) - an AL0 small/invalid enough that no sane turns count could reach the target even at zero
        //bias. This is now the only way this branch can fail to converge - turns is solved directly from AL0 with no iteration, so there is no "runaway" case left to guard against.
        result.converged = false;
        result.rejectionReasons.push_back("required turns count is unreasonably large for core '" + core.partNumber + "' at this target inductance - zero-bias catalog AL0 is too small to satisfy the target with a sane winding");
        return result;
    }

    //get the zero bias inductance and error percent which is the current at 0 A DC bias, not the real operating current. This is the only turns count the solver ever sees, and it is fixed for the rest of this function.
    double zeroBiasNh = static_cast<double>(turns) * turns * core.al;
    double errorPercent = 100.0 * (zeroBiasNh - targetNh) / targetNh;
    result.turns = turns;
    //Identical to turns now (turns is never raised past the zero-bias solve) - kept as its own field
    result.zeroBiasSeedTurns = turns;
    result.gapMm = 0.0;
    //set to the rolled-off (loaded) AL below once the DC-bias block runs - core.al (zero-bias) is only the placeholder default here for the no-rolloff-data case, matching this field's existing name/
    //meaning ("effective AL at operating current"), not the zero-bias design AL.
    result.effectiveAlNHPerTurnSquared = core.al;
    result.calculatedInductanceUH = units::nHToUh(zeroBiasNh);
    result.inductanceErrorPercent = errorPercent;
    result.withinTolerance = std::abs(errorPercent) <= tolerancePercent;
    result.converged = true;
    result.gapMinMm = 0.0;
    result.gapMaxMm = 0.0;
    result.inductanceAtMinGapUH = result.calculatedInductanceUH;
    result.inductanceAtMaxGapUH = result.calculatedInductanceUH;
    result.inductanceWithinToleranceAcrossGapRange = result.withinTolerance;

    //DC-bias roll-off, evaluated ONCE at this now-fixed turns count - informational (loadedInductanceUH/dcMagnetizingForceOe/percentInitialPermeabilityAtOperatingCurrent), never fed
    //back into turns. loadedInductanceUH is the real physical inductance at the request's real operating current - PeakFluxValidation/SaturationValidation use THIS, not calculatedInductanceUH
    //(the zero-bias design value), since saturation risk has to be judged against what the core actually does at current, not what it does at 0A.
    DCBiasCurveLookup rolloffCurve = findDCBiasCurve(core.material);
    std::optional<double> biasCurrentA;
    bool biasCurrentIsRmsFloor = false;
    if (peakCurrentA.has_value() && *peakCurrentA > 0.0) {
         biasCurrentA = peakCurrentA;
    } 
    else if (rmsCurrentA > 0.0) {
        biasCurrentA = rmsCurrentA;
        biasCurrentIsRmsFloor = true;
    }

    //boolean: if rolloff curve was found and bias current has a value, then rolloff is applied. If false, then no rolloff is applied and the zero-bias AL is used to calculate the inductance at opeprating current
    bool rolloffApplied = rolloffCurve.found && biasCurrentA.has_value();
    result.usesDCBiasRolloffCurve = rolloffApplied;
    //if true then get the hOe, percentMu, loadedAlNh, loadedNh, and loadedInductanceUH at the operating current. If false then set the dcMagnetizingForceOe to 0, percentInitialPermeabilityAtOperatingCurrent to 100, and loadedInductanceUH to the zero-bias calculated inductance
    if (rolloffApplied) {
        //DC Magnetizing Force H (Oersteds)
        double hOe = dcMagnetizingForceOe(turns, *biasCurrentA, core.leMm);
        //DC-bias roll-off: %initial permeability remaining at this H
        double percentMu = percentInitialPermeability(hOe, rolloffCurve.curve);
        //effective AL at this H, in nH/turn^2
        double loadedAlNh = core.al * (percentMu / 100.0);
        //loaded inductance at this H, in nH
        double loadedNh = static_cast<double>(turns) * turns * loadedAlNh;
        result.dcMagnetizingForceOe = hOe;
        result.percentInitialPermeabilityAtOperatingCurrent = percentMu;
        result.dcBiasRolloffUsedRmsFloor = biasCurrentIsRmsFloor;
        result.effectiveAlNHPerTurnSquared = loadedAlNh;
        result.loadedInductanceUH = units::nHToUh(loadedNh);
    } 
    else {
        result.dcMagnetizingForceOe = 0.0;
        result.percentInitialPermeabilityAtOperatingCurrent = 100.0;
        result.dcBiasRolloffUsedRmsFloor = false;
        //No curve/no current available - the zero-bias value is the best known estimate of the operating-current inductance (same convention PeakFluxValidation/SaturationValidation already
        //rely on for non-distributed-gap cores, which have no separate 0-bias/loaded distinction).
        result.loadedInductanceUH = result.calculatedInductanceUH;
    }

    //if the calculated inductance is outside the tolerance, then add a rejection reason to the result
    if (!result.withinTolerance) {
        result.rejectionReasons.push_back("zero-bias calculated inductance " + std::to_string(result.calculatedInductanceUH) +
            " uH (turns=" + std::to_string(turns) + " against catalog AL0 " + std::to_string(core.al) +
            " nH/turn^2, distributed-gap core, no machined gap available to fine-tune) is outside the " +
            std::to_string(tolerancePercent) + "% tolerance of the target " + std::to_string(targetInductanceUH) +
            " uH (error " + std::to_string(errorPercent) + "%)");
    }
    return result;
}
//Machined Gap Core
TurnsAndGapResult solveMachinedGapCore(double targetNh, const CoreCandidate& core, const MaterialCandidate& material, double targetInductanceUH, double tolerancePercent, const std::optional<double>& peakCurrentA, double rmsCurrentA, const DesignRules& rules) {
    TurnsAndGapResult result;
    //effective core area is converted from square millimeters to square centimeters (1cm2 = 100m2)
    double aeCm2 = units::mm2ToCm2(core.aeMm2);
    //magnetic path length is converted from millimeters to centimeters (1cm = 10mm)
    double leCm = units::mmToCm(core.leMm);
    //this calculates the practical gap limit as a fraction of the core's magnetic path length
    double maxGapMm = rules.maxGapFraction * core.leMm;

    int turns = seedTurns(core, targetInductanceUH);

    //Flux-aware turns floor: the plain inductance-matching seed above is the MINIMUM possible turns count for this target L (it comes from the core's ungapped/maximum AL) - it has no awareness of peak current
    //or flux density, so on many real ferrite cores it converges directly to a zero-gap, minimum-turns design that PeakFluxValidation/SaturationValidation then reject downstream with no retry (a real user
    //report, at the time reproduced against the E100/60/28-3C90 ferrite catalog row this project carried then: at 3000uH/5A peak converged at turns=20/gapMm=0.0, Bpk=1.03T vs Bmax=0.47T, when a real
    //~49-turn/~0.6mm-gap design exists on the SAME core and passes; that row has since been removed along with the rest of ferrite scope, but the underlying methodology bug and fix described here are unchanged
    //and still apply to any machined-gap core). When a real peak current is known, raise the starting turns to whatever this exact core/target/limit combination requires to
    //already respect rules.minimumSaturationMarginPercent - Bpk = L*Ipk/(N*Ae) inverted for N. This only ever RAISES the seed (max(), never lowered) - a core where the inductance-matching seed already had
    //enough margin is completely unaffected.
    
    //When peakCurrentA is absent but rmsCurrentA > 0.0, the same seed runs against rmsCurrentA instead - a mathematically guaranteed LOWER BOUND on the real (unsupplied) peak current (RMS <= peak always, for
    //any unidirectional inductor-current waveform), never a substitute for the real peak. Without this, an RMS-only request has NO flux-aware floor at all, and the plain inductance-matching seed can converge on
    //a minimum-turns, ultra-high-permeability ferrite design that saturates far below the stated RMS current Using the RMS floor here does not confirm the design is safe (real ripple could still push the true
    //peak, and therefore true required margin, higher) - PeakFluxValidation/SaturationValidation's own RMS-floor certain-failure check (see DesignValidation.cpp) is what actually rejects a design that's
    //still broken even at this floor; this seed just stops the solver from handing that design a trivial minimum-turns win in the first place.
    std::optional<double> fluxAwareSeedCurrentA;
    bool seedIsRmsFloor = false;
    if (peakCurrentA.has_value() && *peakCurrentA > 0.0) {
        fluxAwareSeedCurrentA = peakCurrentA;
    } 
    else if (rmsCurrentA > 0.0) {
        fluxAwareSeedCurrentA = rmsCurrentA;
        seedIsRmsFloor = true;
    }

    //if flux aware seedCurrent has a value (which it will then go in here) but it can either be peak current or rms current
    if (fluxAwareSeedCurrentA.has_value()) {
        FluxLimit limit = applicableFluxLimit(material, rules);
        int fluxAwareMinTurns = minimumTurnsForSaturationMargin(core, targetInductanceUH, *fluxAwareSeedCurrentA, limit.limitT, rules);
        if (fluxAwareMinTurns > turns) {
            result.turnsRaisedForSaturationMargin = true;
            result.turnsRaisedUsingRmsFloor = seedIsRmsFloor;
            result.turnsRaisedForSaturationMarginReason =
                "seed turns raised from " + std::to_string(turns) + " (inductance-matching minimum, ungapped AL) to " +
                std::to_string(fluxAwareMinTurns) + " turns to respect the " +
                std::to_string(rules.minimumSaturationMarginPercent) + "% saturation margin at " + (seedIsRmsFloor ? "RMS current (used as a guaranteed lower bound on peak current, since no real peak current was supplied) " : "peak current ") +
                std::to_string(*fluxAwareSeedCurrentA) + " A against the applicable " + std::to_string(limit.limitT) + " T flux limit (" + (limit.usedDefault ? "Phase 1 default" : "material-specific") +
                ") - PeakFluxValidation/SaturationValidation still verify this independently below" + (seedIsRmsFloor ? "; the real (unsupplied) peak current could still require more margin than this floor guarantees" : "");
            turns = fluxAwareMinTurns;
        }
    }

    //will run up to kMaxIterations times during each iteration it: Calculates the required gap for the current turns, Rounds the gap to a manufacturable value
    //Calculates the effective AL with that gap, Recalculates the required turns, and Checks whether the turns stopped changing
    for (int iteration = 0; iteration < kMaxIterations; ++iteration) {
        //given the core, number of turns, and target inductance, what air gap is required?
        double gapCm = calculateRequiredGapCm(turns, aeCm2, leCm, core.mu, targetNh);
        //convert centimeters to millimeters to prevent a negative gap
        double gapMm = std::max(0.0, units::cmToMm(gapCm));
        gapMm = std::round(gapMm / rules.gapStepMm) * rules.gapStepMm;

        //check whether the required gap exceeds the practical limit. If it does, return a rejection reason and set converged=false.
        if (gapMm > maxGapMm) {
            result.converged = false;
            result.rejectionReasons.push_back("required gap " + std::to_string(gapMm) + " mm exceeds the practical bound (" + std::to_string(maxGapMm) + " mm, " + std::to_string(rules.maxGapFraction * 100.0) + "% of the core's magnetic path length) for core '" + core.partNumber + "'");
            return result;
        }

        double alEff = calculateEffectiveAlNhPerTurnSq(aeCm2, leCm, core.mu, units::mmToCm(gapMm));
        //if zero or negative effective AL is calculated, return a rejection reason and set converged=false.
        if (alEff <= 0.0) {
            result.converged = false;
            result.rejectionReasons.push_back("effective AL computed as non-positive for core '" + core.partNumber + "' - cannot design turns/gap");
            return result;
        }
        if (targetNh / alEff > kMaxTurnsSquared) {
            //effective AL is small enough that no sane turns count could reach the target - same non-convergence outcome as the <=0.0 case above, not an overflowed sqrt/int cast (which
            //previously silently produced turns=2147483647 instead of an honest rejection here).
            result.converged = false;
            result.rejectionReasons.push_back("required turns count is unreasonably large for core '" + core.partNumber + "' at this target inductance/gap - effective AL is too small to satisfy the target with a sane winding");
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
            //Machined-gap (ferrite) cores have no separate 0-bias/loaded-at-current distinction - the gap loop already solves directly for the real requested operating point, so the "loaded" value
            //PeakFluxValidation/SaturationValidation read is the same as the design value.
            result.loadedInductanceUH = result.calculatedInductanceUH;

            if (!result.withinTolerance) {
                result.rejectionReasons.push_back("calculated inductance " + std::to_string(result.calculatedInductanceUH) + " uH is outside the " + std::to_string(tolerancePercent) + "% tolerance of the target " +
                    std::to_string(targetInductanceUH) + " uH (error " + std::to_string(errorPercent) + "%)");
            }

            //gap-tolerance sweep: a nominal gap that lands within tolerance can still fail once realistic mechanical tolerance is accounted for - checked at both extremes, turns held fixed.
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
                std::string whichExtreme = !minGapWithinTolerance && !maxGapWithinTolerance ? "both the min and max" : !minGapWithinTolerance ? "the min" : "the max";
                result.rejectionReasons.push_back("gap tolerance +-" + std::to_string(rules.gapTolerancePercent) +
                    "% pushes calculated inductance outside the requested tolerance at " + whichExtreme + " gap extreme (" +
                    std::to_string(result.gapMinMm) + " mm to " + std::to_string(result.gapMaxMm) + " mm)");
            }

            //small-gap manufacturability warning - a caveat, not a rejection, since the design is still physically valid, just harder to reliably machine/lap by hand.
            if (gapMm > 0.0 && gapMm < rules.minManufacturableGapMm) {
                result.smallGapWarning = true;
                result.smallGapWarningReason = "calculated gap " + std::to_string(gapMm) + " mm is below the Phase 1 minimum-manufacturable-gap estimate (" +
                    std::to_string(rules.minManufacturableGapMm) + " mm) - a machined/lapped gap this small may not be reliably reproducible";
            }
            return result;
        }
        //update the turns
        turns = newTurns;
    }
    result.converged = false;
    result.rejectionReasons.push_back("turns/gap did not converge within " + std::to_string(kMaxIterations) + " iterations for core '" + core.partNumber + "'");
    return result;
}

//precondition: core.aeMm2 > 0, core.leMm > 0, core.mu > 0, targetInductanceUH > 0
//postcondition: returns turns/gap that realize targetInductanceUH on this core within tolerancePercent, or converged=false with rejectionReasons explaining why. When peakCurrentA is supplied, the starting
//turns count is raised (never lowered) above the plain inductance-matching minimum when needed to respect rules.minimumSaturationMarginPercent - see TurnsAndGapResult::turnsRaisedForSaturationMargin.
//When peakCurrentA is absent but rmsCurrentA > 0.0, the same seed uses rmsCurrentA as a mathematically guaranteed LOWER BOUND on the real (unsupplied) peak current (RMS <= peak always, for any unidirectional
//inductor-current waveform) - never a substitute for the real peak, only a floor that keeps the solver away from designs that are already broken in the best possible case. rmsCurrentA <= 0.0 with no
//peakCurrentA leaves the seed exactly as it was before this parameter existed.
TurnsAndGapResult designTurnsAndGap(const CoreCandidate& core, const MaterialCandidate& material, double targetInductanceUH, double tolerancePercent, const std::optional<double>& peakCurrentA, double rmsCurrentA, const DesignRules& rules) {
    TurnsAndGapResult result;

    //real powder materials (MPP/Kool Mu/High Flux/XFlux/Edge/Sendust, etc.) achieve their working permeability through gapping distributed at the powder-particle level, baked into the catalog AL -
    //there is no discrete machined air gap to report, REGARDLESS OF SHAPE. This used to also require coreShape=="Toroid", which was wrong in the other direction from the original ferrite-toroid bug: real
    //powder E-cores/blocks/EQ/LP families (e.g. Kool Mu E/U/EER, XFlux Blocks) are just as distributed-gap as powder toroids - same material, same physics, different mechanical shape
    bool isDistributedGapCore = core.materialType == "powder";
    result.gapMethod = isDistributedGapCore ? GapMethod::Distributed : rules.gapMethod;

    //only MachinedCenterLeg has a validated formula in Phase 1 (see GapMethod.h) - any other requested method is rejected here rather than having the one validated formula silently applied to a technique
    //it was never checked against. Distributed-gap toroids are exempt from this gate since they never run the discrete-gap formula at all - see below.
    if (!isDistributedGapCore && rules.gapMethod != GapMethod::MachinedCenterLeg) {
        result.converged = false;
        result.rejectionReasons.push_back("gap method is not implemented in Phase 1 (only MachinedCenterLeg has a validated formula)");
        return result;
    }

    //target inductance is converted from microhenries to nanohenries (1uH = 1000nH)
    double targetNh = units::uHToNh(targetInductanceUH);

    //distributed-gap (powder) cores: even on shapes with a physical center leg (E-cores, blocks, EQ/LP), that leg is never machined with an air gap - the material itself provides the gap-equivalent property,
    //distributed through the powder. So the McLyman required-gap iteration below (which solves for an ADDITIONAL air gap on top of the ungapped AL) does not apply - the only lever available is turns.
    //Reporting a nonzero "gap" here would imply a machinable dimension that does not physically exist on this part. No gap-tolerance sweep either - there is no gap dimension for mechanical tolerance to act
    //on; AL manufacturing tolerance is a different, real concern this Phase 1 dataset does not carry data for (see DATA_FILES.md).
    //core.al is the material's INITIAL permeability (measured at ~0 A DC bias), not a constant - real powder cores roll off permeability as DC current rises (see PermeabilityRolloff.h)
    
    //if the material is powder then it takes in distrbuted gap
    if(isDistributedGapCore) {
        return solveDistributedGapCore(targetNh, core, targetInductanceUH, tolerancePercent, peakCurrentA, rmsCurrentA);
    }
    //else if  no distrbuted gap then return this
    return solveMachinedGapCore(targetNh, core, material, targetInductanceUH, tolerancePercent, peakCurrentA, rmsCurrentA, rules);
}
