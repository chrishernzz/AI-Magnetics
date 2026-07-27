#pragma once
#include <string>
#include <vector>
#include "core/sizing/CoreEvaluation.h"
#include "core/losses/LossEvaluation.h"
#include "core/sizing/MaterialEvaluation.h"
#include "core/model/RejectionReason.h"
#include "core/thermal/ThermalEvaluation.h"
#include "core/magnetics/TurnsAndGapDesign.h"
#include "core/winding/WindingDesign.h"
#include "core/losses/SkinDepthRisk.h"
#include "core/model/LossSummary.h"
#include "validation/Validation.h"
#include "validation/DesignValidation.h"
#include "validation/RecommendationStatus.h"

/*

One fully evaluated material+core pairing, carrying every stage's
result plus an overall pass/fail (spec sections 8-10): a candidate is
never labeled "recommended" until turns/gap, magnetic validation,
winding, and loss/thermal evaluation have all run.

*/
struct InductorCandidate {
    MaterialCandidate material;
    CoreCandidate core;
    TurnsAndGapResult turnsAndGap;
    std::vector<ValidationResult> validations;
    WindingDesignResult winding;
    LossEvaluationResult losses;
    ThermalEvaluationResult thermal;

    bool passed = false;
    std::vector<RejectionReason> rejectionReasons;

    //informational flux-limit breakdown for this candidate's material - see DesignValidation.h. Does not change
    //what PeakFluxValidation/SaturationValidation pass/fail on; those two checks remain the actual gate.
    FluxLimitTiers fluxLimits;

    //qualitative skin-depth AC-loss risk for the selected winding - see SkinDepthRisk.h. Not itself a gate in
    //Phase 1, though a High risk level factors into the 3-tier recommendation classification.
    SkinDepthRiskResult acLossRisk;

    //3-tier recommendation classification (spec section 10) - see RecommendationStatus.h. tier == Rejected
    //always mirrors `passed` above; never contradicts it.
    RecommendationClassification recommendation;

    //"Known Evaluated Loss" naming (spec section 11) - see LossSummary.h. isCompleteTotal is permanently false.
    LossSummary lossSummary;

    //documented simple composite of the two concrete manufacturability signals this pipeline actually
    //produces: physical-fill headroom (WindingDesign.h) and the small-gap manufacturability warning
    //(TurnsAndGapDesign.h) - not a new fabricated metric. See InductorDesignService.cpp for the formula.
    double manufacturabilityMarginPercent = 0.0;

    //human-readable per-candidate ranking explanation - see RankingExplanationService.h.
    std::string rankingExplanation;
};
