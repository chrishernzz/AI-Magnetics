#pragma once
#include <optional>
#include <string>
#include "core/sizing/CoreEvaluation.h"
#include "core/sizing/MaterialEvaluation.h"
#include "core/thermal/ThermalEvaluation.h"
#include "core/magnetics/TurnsAndGapDesign.h"
#include "core/winding/WindingDesign.h"
#include "core/model/InductorRequirements.h"
#include "rules/DesignRules.h"
#include "Validation.h"

/*

STAGE 6: Magnetic validation
Six specific checks (spec section 10) instead of one vague pass/fail flag. Each returns its own ValidationResult so a rejected candidate can
report every failed check, not just the first. A default DesignRules limit used in place of missing material-specific data is always flagged
via ValidationResult::usesDefaultAssumption - never presented as fact.

*/
//checks that peakCurrentA/rmsCurrentA/rippleCurrentPeakToPeakA describe one physically consistent waveform(spec: current-consistency validation). 
//NotEvaluated whenever peak or ripple is missing, OR when both are present but genuinely contradict each other (implied DCM, or an out-of-envelope rmsCurrentA) -
//RequirementDerivationService::derive() never throws on this anymore, it always produces a real
//OperatingPoint with a clear currentConsistencyExplanation either way, so a design can still be generated from whatever data is actually usable.
ValidationResult CurrentConsistencyValidation(const OperatingPoint& operatingPoint);

//check turnsAndGap.withinTolerance/converged against tolerancePercent.
ValidationResult InductanceValidation(const TurnsAndGapResult& turnsAndGap, double tolerancePercent);

//computes peak flux density Bpk = L*Ipk/(N*Ae) and checks it against the applicable flux density limit (material-specific BmaxT if available, otherwise rules.defaultFluxDensityLimitT). NotEvaluated when peakCurrentA is absent - never substituted from RMS.
ValidationResult PeakFluxValidation(const CoreCandidate& core, const MaterialCandidate& material, const TurnsAndGapResult& turnsAndGap, const std::optional<double>& peakCurrentA, const DesignRules& rules);

//checks that the margin between the applicable flux density limit and the calculated peak flux density meets rules.minimumSaturationMarginPercent. NotEvaluated when peakCurrentA is absent.
ValidationResult SaturationValidation(const CoreCandidate& core, const MaterialCandidate& material, const TurnsAndGapResult& turnsAndGap, const std::optional<double>& peakCurrentA, const DesignRules& rules);

//checks winding.fillFactor against rules.maximumFillFactor.
ValidationResult WindingFitValidation(const WindingDesignResult& winding, const DesignRules& rules);

//checks winding.currentDensityAperMm2 against rules.allowableCurrentDensityAperCm2.
ValidationResult CurrentDensityValidation(const WindingDesignResult& winding, const DesignRules& rules);

//checks winding.bundleWidthMm (parallel-strand bundle laid side by side) against
//winding.narrowestWindowOpeningMm - only possible when winding.bundleFitStatus == Evaluated (a
//parallel-strand winding on a two-piece core with real window width/height). NotEvaluated for a
//single-strand winding or any core with no real linear window dimension (every toroid) - never assumed to fit.
ValidationResult BundleFitValidation(const WindingDesignResult& winding);

//checks thermal.predictedTempRiseC against allowableTempRiseC when thermal.status == Evaluated; always not_evaluated (passed=false, never an assumed pass) in Phase 1 since evaluateThermal() never produces an
// Evaluated result today.
ValidationResult ThermalValidation(const ThermalEvaluationResult& thermal, double allowableTempRiseC);

/*
Flux-limit tiers (spec section 3): the single flux limit PeakFluxValidation/SaturationValidation already gate
on, broken into named tiers instead of one anonymous number - informational, does not change what those two
checks pass/fail on. Only 2 of 4 tiers are real in Phase 1; see the struct fields below and the Honesty Ledger
in the project plan for why the other two are permanently not_evaluated rather than a guessed number.
*/
struct FluxLimitTiers {
    //same value PeakFluxValidation/SaturationValidation already check Bpk against - material BmaxT if known, else the Phase 1 default (rules.defaultFluxDensityLimitT). Not a second, independent limit.
    double absoluteSaturationT = 0.0;
    bool absoluteSaturationIsDefault = false;

    //absoluteSaturationT * rules.recommendedFluxDerateFactor - a real computed derating, not a separate measurement.
    double recommendedOperatingT = 0.0;

    //Permanently NotEvaluated in Phase 1 - no temperature-coefficient-vs-saturation data exists anywhere in the
    //current material database to compute a temperature-adjusted saturation limit from.
    EvaluationStatus temperatureAdjustedStatus = EvaluationStatus::NotEvaluated;
    std::string temperatureAdjustedExplanation = "no temperature-coefficient-vs-saturation data exists in the current material database";

    //Permanently NotEvaluated in Phase 1 - no core-loss-vs-flux-density sweep data exists to derive a
    //loss-limited flux ceiling from (separate from the single-point Steinmetz coefficient lookup LossEvaluation
    //already uses to compute core-loss wattage).
    EvaluationStatus coreLossLimitedStatus = EvaluationStatus::NotEvaluated;
    std::string coreLossLimitedExplanation = "no core-loss-vs-flux-density sweep data exists to derive a loss-limited flux ceiling";
};

//precondition: none
//postcondition: returns the four named flux-limit tiers (spec section 3) for the given material/rules, with the two permanently-not-evaluated tiers always NotEvaluated regardless of which material/rules are passed in.
FluxLimitTiers calculateFluxLimitTiers(const MaterialCandidate& material, const DesignRules& rules);
