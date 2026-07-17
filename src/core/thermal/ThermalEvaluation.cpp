#include "core/thermal/ThermalEvaluation.h"

// precondition: none
// postcondition: always returns not_evaluated in Phase 1 (see header)
ThermalEvaluationResult evaluateThermal() {
    ThermalEvaluationResult result;
    result.status = EvaluationStatus::NotEvaluated;
    result.missingDataExplanation =
        "no thermal-resistance model or data is available in Phase 1 (neither CSV "
        "carries Rth or surface-area data)";
    return result;
}
