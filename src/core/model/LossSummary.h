#pragma once
#include <string>

/*

STAGE 11: Loss naming (spec section 11)

Replaces the ambiguous old "total loss" concept with an explicit statement of what's actually known.
isCompleteTotal is permanently false in Phase 1 - core/losses/SkinDepthRisk.h's acLossWattsStatus never
becomes Evaluated (no AC-loss watts model exists), so no candidate can ever report a genuinely complete
total, only the sum of whichever loss components (copper, core) are actually known. A direct, intentional
consequence of not having an AC-watts model, not an oversight - see RecommendationStatus.h for the
analogous "structurally unreachable" reasoning applied to Phase1Recommended.

*/
struct LossSummary {
    //sum of copper/core loss for whichever of the two are Evaluated - never a silently-assumed 0.0 for a
    //missing component (see InductorDesignService.cpp's buildLossSummary()).
    double knownEvaluatedLossW = 0.0;
    bool isCompleteTotal = false;
    //human-readable label naming exactly which components are included, e.g. "Known Evaluated Loss
    //(copper+core - partial coverage, AC/skin-effect loss not modeled)".
    std::string label;
};
