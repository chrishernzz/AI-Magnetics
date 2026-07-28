#pragma once
#include <string>
#include <vector>
#include "validation/EvaluationStatus.h"
#include "rules/DesignRules.h"
#include "core/sizing/CoreEvaluation.h"
#include "core/winding/WindingConstructionType.h"

/*

STAGE 7: Winding design (round copper wire, Phase 1)
Fill factor and current density only need turns x wire cross-section area
window area, so they are always computed. Total wire length and DCR
need mean-length-per-turn (CoreCandidate::mltMm) - real for cores whose
upstream geometry supports the estimate (see scripts/export_real_data.py),
reported not_evaluated with an explicit missing-data explanation for any
core where it's still 0.0, rather than an invented number (spec section
11: "do NOT invent values silently").

Physical-fill model (spec section 5): the raw copper-only fillFactor/fitsWindow
below undercounts real winding space - insulation build-up, achievable packing
density, bobbin wall thickness, and margin/lead-exit clearance all reduce how
much of a core's window a winding can actually occupy. physicalWindowFillFactor/
fitsPhysicalWindow are the realistic figures WindingFitValidation gates on;
fillFactor/fitsWindow remain as an informational "raw copper fill" figure.

*/

struct WindingDesignResult {
    std::string wireDescription;
    //cross-section area of ONE strand of the selected AWG (bare copper, used for DCR)
    double conductorAreaMm2 = 0.0;
    int parallelStrands = 1;
    //raw copper-area-over-window-area figure - informational only; WindingFitValidation gates on
    //physicalWindowFillFactor below, not this.
    double fillFactor = 0.0;
    double currentDensityAperMm2 = 0.0;
    //raw copper fill vs rules.maximumFillFactor - informational only, see fillFactor above.
    bool fitsWindow = false;

    EvaluationStatus resistanceStatus = EvaluationStatus::NotEvaluated;
    //valid only when resistanceStatus == Evaluated - total bare-copper length across all parallel strands
    //for the core-winding portion only (turns x MLT x parallelStrands), same meaning as before this commit.
    double totalWireLengthM = 0.0;
    //valid only when resistanceStatus == Evaluated - now includes lead/routing/connection resistance (see
    //coldDcrOhmsAt20C below), not just core-winding resistance - an intentional realism improvement, not a
    //silent behavior change: any consumer reading this field now sees the more complete number.
    double dcrOhms = 0.0;
    std::vector<std::string> missingData;

    //--- Construction (spec section 5) ---
    WindingConstructionType constructionType = WindingConstructionType::SingleRoundWire;
    //bare strand diameter + rules.singleBuildInsulationBuildUpMm
    double insulatedConductorDiameterMm = 0.0;
    //circular cross-section area of ONE insulated strand (physical space it occupies, not electrical area)
    double insulatedConductorAreaMm2 = 0.0;
    //human-readable one-turn/bundle description, e.g. "4x AWG18 magnet wire (bare 1.02 mm, insulated ~1.07 mm each), wound as a single bundle"
    std::string physicalDescription;

    //--- Physical window fill (see block comment above) ---
    double physicalWindowAreaMm2 = 0.0;
    double physicalWindowFillFactor = 0.0;
    bool fitsPhysicalWindow = false;
    //currentDensityAperMm2 derated by rules.currentSharingDerateFactor when parallelStrands > 1 - informational,
    //does not itself gate CurrentDensityValidation.
    double effectiveCurrentDensityAperMm2 = 0.0;
    //permanently NotEvaluated in Phase 1 - core data has only a raw window area, never a width/height split,
    //so there is no linear opening dimension to check a parallel-strand bundle against.
    EvaluationStatus bundleFitStatus = EvaluationStatus::NotEvaluated;

    //--- DCR / copper loss realism (spec section 6) ---
    //single-strand path length around the core (turns x MLT), meters - valid only when resistanceStatus == Evaluated.
    double coreWindingLengthM = 0.0;
    //rules.totalLeadLengthAllowanceMm converted to meters - a Phase 1 generic estimate, not per-core measured data.
    double leadLengthM = 0.0;
    //rules.routingLengthAllowanceMm converted to meters.
    double routingLengthM = 0.0;
    //coreWindingLengthM + leadLengthM + routingLengthM (single-strand basis, before the parallel-strand resistance division).
    double totalLengthM = 0.0;
    //rules.connectionResistanceMilliOhm converted to ohms - a single termination joint, not divided by parallelStrands.
    double connectionResistanceOhms = 0.0;
    //valid only when resistanceStatus == Evaluated - resistance at 20C, same value now aliased as dcrOhms above.
    double coldDcrOhmsAt20C = 0.0;
    //conservative fallback hot-DCR estimate using rules.assumedWindingTempCWhenThermalNotEvaluated - NOT a
    //converged answer. Overwritten with the real iterative thermal loop's converged value once that loop
    //exists and converges (see ThermalEvaluation.h) - until then this is the honest, clearly-a-sanity-check number.
    double estimatedHotDcrOhms = 0.0;
};

//precondition: core.waMm2 > 0, turns > 0, rmsCurrentA > 0
//postcondition: selects a round-wire gauge (or parallel strands of rules.minimumSingleStrandAwg if a single strand would be impractically thick) meeting rules.allowableCurrentDensityAperCm2, and computes fill
//factor / current density against the core's actual window area.
WindingDesignResult designWinding(const CoreCandidate& core, int turns, double rmsCurrentA, const DesignRules& rules);
