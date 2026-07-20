#pragma once
#include <string>
#include "EvaluationStatus.h"

/*

STAGE 6: Magnetic validation result
Same purpose as the original stub comment described: checking results
against physical limits (flux density vs. Bsat, window fill factor,
allowable temperature rise) - now with the specific per-check fields
spec section 10 asks for, instead of a single unexplained bool.

*/
struct ValidationResult {
    bool passed;
    std::string checkName;
    double calculatedValue;
    double limitValue;
    std::string unit;
    std::string explanation;

    //true if this check compared against DesignRules' Phase 1 default (e.g. defaultFluxDensityLimitT) rather than a material-specific measured value - so a default is never silently presented as fact.
    bool usedDefaultLimit = false;

    //Evaluated: the check actually ran and passed/failed on real data.
    //NotEvaluated: required data/model wasn't available, so `passed` is false per spec ("never assume missing data equals a pass"), but a
    //candidate is only blocked by checks that actually ran and failed - a NotEvaluated check is a caveat to surface, not a rejection reason
    //(see InductorDesignService's aggregation).
    EvaluationStatus status = EvaluationStatus::Evaluated;
};
