#pragma once
#include "core/sizing/CoreEvaluation.h"
#include "core/sizing/MaterialEvaluation.h"
#include "core/thermal/ThermalEvaluation.h"
#include "core/magnetics/TurnsAndGapDesign.h"
#include "core/winding/WindingDesign.h"
#include "rules/DesignRules.h"
#include "Validation.h"

// STAGE 6: Magnetic validation
//
// Six specific checks (spec section 10) instead of one vague pass/fail
// flag. Each returns its own ValidationResult so a rejected candidate can
// report every failed check, not just the first. A default DesignRules
// limit used in place of missing material-specific data is always flagged
// via ValidationResult::usedDefaultLimit - never presented as fact.

// Checks turnsAndGap.withinTolerance/converged against tolerancePercent.
ValidationResult InductanceValidation(const TurnsAndGapResult& turnsAndGap, double tolerancePercent);

// Computes peak flux density Bpk = L*Ipk/(N*Ae) and checks it against the
// applicable flux density limit (material-specific BmaxT if available,
// otherwise rules.defaultFluxDensityLimitT).
ValidationResult PeakFluxValidation(const CoreCandidate& core, const MaterialCandidate& material,
                                     const TurnsAndGapResult& turnsAndGap, double peakCurrentA,
                                     const DesignRules& rules);

// Checks that the margin between the applicable flux density limit and the
// calculated peak flux density meets rules.minimumSaturationMarginPercent.
ValidationResult SaturationValidation(const CoreCandidate& core, const MaterialCandidate& material,
                                       const TurnsAndGapResult& turnsAndGap, double peakCurrentA,
                                       const DesignRules& rules);

// Checks winding.fillFactor against rules.maximumFillFactor.
ValidationResult WindingFitValidation(const WindingDesignResult& winding, const DesignRules& rules);

// Checks winding.currentDensityAperMm2 against rules.allowableCurrentDensityAperCm2.
ValidationResult CurrentDensityValidation(const WindingDesignResult& winding, const DesignRules& rules);

// Checks thermal.predictedTempRiseC against allowableTempRiseC when
// thermal.status == Evaluated; always not_evaluated (passed=false, never
// an assumed pass) in Phase 1 since evaluateThermal() never produces an
// Evaluated result today.
ValidationResult ThermalValidation(const ThermalEvaluationResult& thermal, double allowableTempRiseC);
