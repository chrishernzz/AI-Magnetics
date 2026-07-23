#pragma once
#include <vector>
#include "core/sizing/CoreEvaluation.h"
#include "core/losses/LossEvaluation.h"
#include "core/sizing/MaterialEvaluation.h"
#include "core/model/RejectionReason.h"
#include "core/thermal/ThermalEvaluation.h"
#include "core/magnetics/TurnsAndGapDesign.h"
#include "core/winding/WindingDesign.h"
#include "validation/Validation.h"

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
};
