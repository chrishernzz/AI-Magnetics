#include "LossEvaluation.h"
#include "CopperLoss.h"

// precondition: see header
// postcondition: see header
LossEvaluationResult evaluateLosses(const MaterialCandidate& material, const WindingDesignResult& winding,
                                     double rmsCurrentA, double switchingFreqHz) {
    (void)switchingFreqHz;  // reserved for Stage B once real per-material coefficients exist
    LossEvaluationResult result;

    if (winding.resistanceStatus == EvaluationStatus::Evaluated) {
        result.copperLossStatus = EvaluationStatus::Evaluated;
        result.copperLossW = calculateCopperLoss(rmsCurrentA, winding.dcrOhms);
    } else {
        result.copperLossStatus = EvaluationStatus::NotEvaluated;
        result.missingData.push_back("DC copper loss not evaluated: " +
                                      (winding.missingData.empty() ? std::string("DCR unavailable")
                                                                    : winding.missingData.front()));
    }

    // Core loss (Stage B): only when the material carries a validated
    // core-loss coefficient - never true for real_materials.csv today.
    result.coreLossStatus = EvaluationStatus::NotEvaluated;
    result.missingData.push_back("core loss not evaluated: material '" + material.materialFamily +
                                  "' has no validated core-loss coefficients");

    // Skin/proximity (high-frequency) loss: not implemented in Phase 1.
    result.highFrequencyLossStatus = EvaluationStatus::NotEvaluated;
    result.missingData.push_back(
        "high-frequency (skin/proximity) loss model is not implemented in Phase 1");

    return result;
}
