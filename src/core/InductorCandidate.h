#pragma once
#include <vector>
#include "CoreEvaluation.h"
#include "LossEvaluation.h"
#include "MaterialEvaluation.h"
#include "RejectionReason.h"
#include "ThermalEvaluation.h"
#include "TurnsAndGapDesign.h"
#include "WindingDesign.h"
#include "../validation/Validation.h"

// One fully evaluated material+core pairing, carrying every stage's
// result plus an overall pass/fail (spec sections 8-10): a candidate is
// never labeled "recommended" until turns/gap, magnetic validation,
// winding, and loss/thermal evaluation have all run.
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
};
