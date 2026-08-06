#pragma once
#include <optional>
#include <string>
#include <vector>
#include "validation/EvaluationStatus.h"
#include "core/sizing/CoreEvaluation.h"
#include "core/sizing/MaterialEvaluation.h"
#include "core/magnetics/TurnsAndGapDesign.h"
#include "core/winding/WindingDesign.h"

/*

STAGE 8: Loss evaluation
Orchestrates CopperLoss (Stage A) and CoreLoss (Stage B) - each stays a focused module, called from here
rather than merged together (spec section 12). Every loss the required data doesn't support is reported
not_evaluated, never silently as 0 W. Skin-depth AC-loss risk is a separate, qualitative-only concern -
see core/losses/SkinDepthRisk.h - not a watts figure alongside these two, so it is not a field here.

*/
struct LossEvaluationResult {
    EvaluationStatus copperLossStatus = EvaluationStatus::NotEvaluated;
    //valid only when copperLossStatus == Evaluated
    double copperLossW = 0.0;

    EvaluationStatus coreLossStatus = EvaluationStatus::NotEvaluated;
    //valid only when coreLossStatus == Evaluated
    double coreLossW = 0.0;

    //--- Core-loss detail (spec section 7) - surfaces what was already computed internally but never exposed. All valid only when coreLossStatus == Evaluated.
    std::string coreLossMaterialUsed;
    //the matched coefficient row's own declared valid frequency range - not the requested switching frequency.
    double coreLossCoefficientMinFreqHz = 0.0;
    double coreLossCoefficientMaxFreqHz = 0.0;
    double coreLossFluxDensitySwingT = 0.0;
    double coreLossVolumeM3 = 0.0;
    double coreLossDensityWPerM3 = 0.0;

    std::vector<std::string> missingData;
};

LossEvaluationResult evaluateLosses(const MaterialCandidate& material, const CoreCandidate& core, const TurnsAndGapResult& turnsAndGap, const WindingDesignResult& winding, double rmsCurrentA, double switchingFreqHz, const std::optional<double>& rippleCurrentPeakToPeakA);
