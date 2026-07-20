#pragma once
#include <string>
#include <vector>
#include "validation/EvaluationStatus.h"
#include "rules/DesignRules.h"
#include "core/sizing/CoreEvaluation.h"

/*

STAGE 7: Winding design (round copper wire, Phase 1)
Fill factor and current density only need turns x wire cross-section area
window area, so they are always computed. Total wire length and DCR
need mean-length-per-turn (CoreCandidate::mltMm) - real for cores whose
upstream geometry supports the estimate (see scripts/export_real_data.py),
reported not_evaluated with an explicit missing-data explanation for any
core where it's still 0.0, rather than an invented number (spec section
11: "do NOT invent values silently").

*/

struct WindingDesignResult {
    std::string wireDescription;
    //cross-section area of ONE strand of the selected AWG
    double conductorAreaMm2 = 0.0; 
    int parallelStrands = 1;
    double fillFactor = 0.0;
    double currentDensityAperMm2 = 0.0;
    bool fitsWindow = false;

    EvaluationStatus resistanceStatus = EvaluationStatus::NotEvaluated;
    //valid only when resistanceStatus == Evaluated
    double totalWireLengthM = 0.0; 
    //valid only when resistanceStatus == Evaluated
    double dcrOhms = 0.0;        
    std::vector<std::string> missingData;
};

//precondition: core.waMm2 > 0, turns > 0, rmsCurrentA > 0
//postcondition: selects a round-wire gauge (or parallel strands of rules.minimumSingleStrandAwg if a single strand would be impractically thick) meeting rules.allowableCurrentDensityAperCm2, and computes fill
//factor / current density against the core's actual window area.
WindingDesignResult designWinding(const CoreCandidate& core, int turns, double rmsCurrentA, const DesignRules& rules);
