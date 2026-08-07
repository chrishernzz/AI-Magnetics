#pragma once
#include <string>
#include "validation/EvaluationStatus.h"
#include "rules/DesignRules.h"

/*

STAGE 8c: Skin-depth AC-loss risk (spec section 8, renamed from HighFrequencyLosses)

The old HighFrequencyLosses.{h,cpp} name implied a watts-producing AC-loss model - Phase 1 has never had
one (calculateHighFrequencyLoss() always returned 0.0 and was never even called from LossEvaluation.cpp).
This module replaces that dead stub with a real, but deliberately qualitative, skin-depth heuristic:
Low/Moderate/High risk based on how the selected strand's bare radius compares to the classical skin depth
at the operating frequency. acLossWattsStatus stays permanently NotEvaluated - a risk LEVEL is not a watts
number, and inventing one from this heuristic alone would misrepresent it as a validated AC-loss model.

What this DOES evaluate: single-strand skin effect (conductor diameter vs. skin depth at the switching
frequency). What this does NOT evaluate, named explicitly in `reason` rather than silently folded into the
score: proximity effect between bundled parallel strands, and proximity to the air gap - no winding-layer or
winding-to-gap geometry exists anywhere in this engine to compute either from.

*/

enum class AcLossRiskLevel {
    Low,
    Moderate,
    High,
};

struct SkinDepthRiskResult {
    //classical skin depth at the switching frequency, mm - delta = sqrt(rho_Cu / (pi * f * mu0))
    double skinDepthMm = 0.0;
    //bare-copper radius of the selected strand, mm (one strand, not the parallel bundle)
    double strandRadiusMm = 0.0;
    //strandRadiusMm / skinDepthMm - the heuristic's actual decision variable
    double radiusToSkinDepthRatio = 0.0;
    AcLossRiskLevel riskLevel = AcLossRiskLevel::Low;
    //names what was evaluated (single-strand skin effect) and what was not (bundle proximity effect, proximity to the air gap) - see block comment above.
    std::string reason;

    //permanently NotEvaluated in Phase 1 - see block comment above. A risk level is not a watts figure.
    EvaluationStatus acLossWattsStatus = EvaluationStatus::NotEvaluated;
    std::string acLossWattsExplanation = "no AC-loss watts model is implemented in Phase 1 - this is a qualitative skin-depth risk heuristic only";
};

SkinDepthRiskResult evaluateSkinDepthRisk(double switchingFreqHz, double conductorAreaMm2, const DesignRules& rules);
