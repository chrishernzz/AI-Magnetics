#pragma once
#include <string>
#include "rules/DesignRules.h"

/*

STAGE 9: Thermal evaluation (spec section 9, binding decision #2)

Rewritten from a zero-argument stub that always returned not_evaluated into a real iterative
convergence loop (temp -> hot DCR -> copper loss -> temp rise -> repeat), mirroring
TurnsAndGapDesign.cpp's existing convergence-loop pattern (same seed/iterate/check-stability shape,
same iteration-cap safety net).

ThermalStatus deliberately has no "fully evaluated" value. The thermal resistance used each run is now a
size-aware estimate derived from the candidate's own real core geometry (Ae*Le -> an estimated external
surface area -> Rth = 1/(h*A), Newton's law of cooling - see estimateThermalResistanceCPerW() below and
DesignRules.h for the sourcing of h and the shape-factor simplification), falling back to the flat
rules.defaultThermalResistanceCPerW only when that geometry is unavailable. Either way it is still an
estimate, never per-core measured or simulated Rth data, so this loop can only ever produce a
PreliminaryThermalEstimate, never a full pass, no matter how complete the upstream loss coverage is.
ThermalValidation (DesignValidation.h) still runs a real pass/fail comparison against allowableTempRiseC
when a PreliminaryThermalEstimate is available - carried by ValidationResult::isPreliminaryEstimate, not a
fake third status.

Positive-feedback note: copper resistance rises with temperature, temperature rises with loss, and loss
rises with resistance - a real feedback loop, not just a numerical convergence exercise. For a low-DCR,
high-current design this loop's local iteration gain (rmsCurrentA^2 * coldDcrOhmsAt20C *
copperTempCoefficientPerC * the Rth estimate in use for this candidate) can reach or exceed 1, in which
case the iteration diverges rather than converges - a real, reachable case (not a fabricated one) for
high-current designs, and the honest reading of a non-converging case here is "this heuristic can't
produce a stable estimate," not "assume it's fine and report the last number anyway." See
evaluateThermal()'s non-converged branch below.

*/

enum class ThermalStatus {
    NotEvaluated,
    PreliminaryThermalEstimate,
};

struct ThermalIterationInputs {
    double ambientTemperatureC = 25.0;
    double rmsCurrentA = 0.0;
    //cold (20C) DCR from WindingDesignResult::coldDcrOhmsAt20C - only meaningful when copperLossGeometryKnown.
    double coldDcrOhmsAt20C = 0.0;
    //mirrors WindingDesignResult::resistanceStatus == Evaluated - without a real DCR to correct with
    //temperature, the loop cannot even seed a hot-DCR estimate.
    bool copperLossGeometryKnown = false;
    double coreLossW = 0.0;
    //mirrors LossEvaluationResult::coreLossStatus == Evaluated - when false, the loop still runs on
    //copper loss alone (a real, if partial, estimate) rather than refusing to run at all.
    bool coreLossKnown = false;

    //CoreCandidate::aeMm2/leMm - this candidate's own real magnetic-circuit geometry, used to derive a
    //size-aware thermal resistance (see estimateThermalResistanceCPerW() in ThermalEvaluation.cpp).
    //0.0 means unknown, in which case the loop falls back to rules.defaultThermalResistanceCPerW.
    double coreEffectiveAreaMm2 = 0.0;
    double coreMagneticPathLengthMm = 0.0;
};

struct ThermalEvaluationResult {
    ThermalStatus status = ThermalStatus::NotEvaluated;

    //--- valid only when status == PreliminaryThermalEstimate ---
    double convergedWindingTempC = 0.0;
    double hotDcrOhms = 0.0;
    double copperLossAtConvergedTempW = 0.0;
    //copperLossAtConvergedTempW + (coreLossW if coreLossKnown) - the loss actually driving this estimate.
    double knownLossW = 0.0;
    double predictedTempRiseC = 0.0;
    //same as convergedWindingTempC - no separate hotspot-vs-average thermal model exists in Phase 1, so
    //this is the winding temperature itself, not an independently modeled hotspot.
    double predictedHotspotTempC = 0.0;
    int iterationsUsed = 0;
    bool converged = false;
    //the thermal resistance actually used for this run - carried alongside the result so a caller never
    //has to re-derive which Rth assumption produced it.
    double thermalResistanceCPerWUsed = 0.0;
    //true when thermalResistanceCPerWUsed came from this candidate's own real Ae/Le geometry (Newton's
    //law of cooling over an estimated compact-solid surface area) rather than the flat
    //rules.defaultThermalResistanceCPerW fallback (used only when geometry was unavailable).
    bool thermalResistanceIsGeometryDerived = false;

    std::string missingDataExplanation;
};

//precondition: none
//postcondition: see block comment above. Iterates up to rules.maxThermalIterations times (mirrors
//TurnsAndGapDesign.cpp's kMaxIterations pattern), converged when the winding-temperature change between
//iterations is below rules.thermalConvergenceThresholdC. Returns NotEvaluated (not a stale intermediate
//number) when copperLossGeometryKnown is false, or when the loop fails to converge within the iteration cap.
ThermalEvaluationResult evaluateThermal(const ThermalIterationInputs& inputs, const DesignRules& rules);
