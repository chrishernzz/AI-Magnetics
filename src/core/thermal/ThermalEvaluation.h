#pragma once
#include <string>
#include "validation/EvaluationStatus.h"

// STAGE 9: Thermal evaluation
//
// Always not_evaluated in Phase 1: neither real_cores.csv nor
// real_materials.csv carries a thermal resistance (Rth) figure or the
// surface-area data a naturally-cooled Rth estimate would need. This
// module exists as a real pipeline stage (spec section 4's orchestration
// list includes "evaluate thermal behavior") rather than being silently
// skipped - ThermalValidation (src/validation/DesignValidation.h) reads
// its result and reports not_evaluated honestly instead of assuming a pass.

struct ThermalEvaluationResult {
    EvaluationStatus status = EvaluationStatus::NotEvaluated;
    double predictedTempRiseC = 0.0;  // valid only when status == Evaluated
    std::string missingDataExplanation;
};

ThermalEvaluationResult evaluateThermal();
