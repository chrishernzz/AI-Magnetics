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
Orchestrates CopperLoss (Stage A), CoreLoss (Stage B), and
HighFrequencyLosses - each stays a focused module, called from here
rather than merged together (spec section 12). Every loss the required
data doesn't support is reported not_evaluated, never silently as 0 W.

*/
struct LossEvaluationResult {
    EvaluationStatus copperLossStatus = EvaluationStatus::NotEvaluated;
    //valid only when copperLossStatus == Evaluated
    double copperLossW = 0.0;  

    EvaluationStatus coreLossStatus = EvaluationStatus::NotEvaluated;
    //valid only when coreLossStatus == Evaluated
    double coreLossW = 0.0;  

    EvaluationStatus highFrequencyLossStatus = EvaluationStatus::NotEvaluated;
    //valid only when coreLossStatus == Evaluated
    double highFrequencyLossW = 0.0;

    std::vector<std::string> missingData;
};

//precondition: none
//postcondition: computes DC copper loss only when winding.resistanceStatus == Evaluated (uses rmsCurrentA, never peak current). Computes core
//loss only when rippleCurrentPeakToPeakA is supplied AND findCoreLossCoefficients() finds a real coefficient row for this material/frequency -
//flux-density swing is computed as Bswing = calculatedInductanceH * ripplePkPkA / (turns * Ae_m2), the same peak-flux relationship
//PeakFluxValidation uses, but driven by the ripple current rather than peak current. High-frequency (skin/proximity) loss is always
//not_evaluated in Phase 1 - no model implemented. See docs/FORMULAS.md section 9 for the full explanation, including why no ripple data means
//no core-loss number (Option 1: never approximate Bswing from peak flux).
LossEvaluationResult evaluateLosses(const MaterialCandidate& material, const CoreCandidate& core, const TurnsAndGapResult& turnsAndGap,
                                     const WindingDesignResult& winding, double rmsCurrentA, double switchingFreqHz,
                                     const std::optional<double>& rippleCurrentPeakToPeakA);
