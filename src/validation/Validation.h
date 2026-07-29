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
    //Defaulted (not left indeterminate) - every real construction path sets these explicitly before use, but
    //a default-constructed instance (e.g. in a test fixture) reading passed/calculatedValue/limitValue before
    //assignment would otherwise be genuine undefined behavior. See MaterialCandidate::hasBmaxData for a real
    //case this exact pattern caused (a g++-11-specific test failure from indeterminate garbage).
    bool passed = false;
    std::string checkName;
    double calculatedValue = 0.0;
    double limitValue = 0.0;
    std::string unit;
    std::string explanation;

    //true if this check compared against DesignRules' Phase 1 default (e.g. defaultFluxDensityLimitT) rather than a material-specific measured value - so a default is never silently presented as fact.
    bool usesDefaultAssumption = false;

    //Evaluated: the check actually ran and passed/failed on real data.
    //NotEvaluated: required data/model wasn't available, so `passed` is false per spec ("never assume missing data equals a pass"), but a
    //candidate is only blocked by checks that actually ran and failed - a NotEvaluated check is a caveat to surface, not a rejection reason
    //(see InductorDesignService's aggregation).
    EvaluationStatus status = EvaluationStatus::Evaluated;

    //true when this check's numeric result rests on a Phase 1 coarse estimate rather than validated/measured
    //data (currently: ThermalValidation when thermal.status == PreliminaryThermalEstimate) - status stays
    //Evaluated (the check DID run and DID pass/fail), this flag is the caveat that it's not a final answer.
    bool isPreliminaryEstimate = false;

    //true when this check is a required gate for determineRecommendationStatus()'s PASS tier - every check
    //this engine runs today is mandatory (spec: "PASS only when every mandatory check was evaluated AND
    //passed"). Exists as an explicit field, not an implicit "everything in the list" assumption, so a future
    //informational-only check can be added without silently becoming a gate.
    bool mandatory = true;
};
