#pragma once
#include <string>
#include <vector>
#include "../validation/EvaluationStatus.h"
#include "MaterialEvaluation.h"
#include "WindingDesign.h"

// STAGE 8: Loss evaluation
//
// Orchestrates CopperLoss (Stage A), CoreLoss (Stage B), and
// HighFrequencyLosses - each stays a focused module, called from here
// rather than merged together (spec section 12). Every loss the required
// data doesn't support is reported not_evaluated, never silently as 0 W.

struct LossEvaluationResult {
    EvaluationStatus copperLossStatus = EvaluationStatus::NotEvaluated;
    double copperLossW = 0.0;  // valid only when copperLossStatus == Evaluated

    EvaluationStatus coreLossStatus = EvaluationStatus::NotEvaluated;
    double coreLossW = 0.0;  // valid only when coreLossStatus == Evaluated

    EvaluationStatus highFrequencyLossStatus = EvaluationStatus::NotEvaluated;
    double highFrequencyLossW = 0.0;  // valid only when highFrequencyLossStatus == Evaluated

    std::vector<std::string> missingData;
};

// precondition: none
// postcondition: computes DC copper loss only when winding.resistanceStatus
// == Evaluated (uses rmsCurrentA, never peak current); computes core loss
// only when material.hasCoreLossData; high-frequency (skin/proximity) loss
// is not implemented in Phase 1 and always reported not_evaluated.
LossEvaluationResult evaluateLosses(const MaterialCandidate& material, const WindingDesignResult& winding,
                                     double rmsCurrentA, double switchingFreqHz);
