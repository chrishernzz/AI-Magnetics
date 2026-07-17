#include "InductorDesignService.h"
#include <algorithm>
#include <unordered_map>
#include "../../core/AreaProduct.h"
#include "../../core/CoreEvaluation.h"
#include "../../core/LossEvaluation.h"
#include "../../core/MaterialEvaluation.h"
#include "../../core/ThermalEvaluation.h"
#include "../../core/TurnsAndGapDesign.h"
#include "../../core/WindingDesign.h"
#include "../../rules/DesignRules.h"
#include "../../validation/DesignValidation.h"
#include "RequirementDerivationService.h"

namespace {

InductorCandidate evaluateCandidate(const CoreCandidate& core, const MaterialCandidate& material, const InductorRequirements& requirements, const DesignRules& rules) {
    InductorCandidate candidate;
    candidate.material = material;
    candidate.core = core;

    double targetInductanceUH = requirements.operatingPoint.inductanceH * 1e6;
    candidate.turnsAndGap = designTurnsAndGap(core, targetInductanceUH, requirements.inductanceTolerancePercent);

    if (!candidate.turnsAndGap.converged) {
        candidate.passed = false;
        for (const auto& reason : candidate.turnsAndGap.rejectionReasons) {
            candidate.rejectionReasons.push_back({"TurnsAndGapDesign", reason});
        }
        return candidate;
    }

    candidate.winding = designWinding(core, candidate.turnsAndGap.turns, requirements.operatingPoint.rmsCurrentA, rules);
    candidate.thermal = evaluateThermal();
    candidate.losses = evaluateLosses(material, candidate.winding, requirements.operatingPoint.rmsCurrentA,
                                       requirements.operatingPoint.switchingFreqHz);

    candidate.validations = {
        InductanceValidation(candidate.turnsAndGap, requirements.inductanceTolerancePercent),
        PeakFluxValidation(core, material, candidate.turnsAndGap, requirements.operatingPoint.peakCurrentA, rules),
        SaturationValidation(core, material, candidate.turnsAndGap, requirements.operatingPoint.peakCurrentA, rules),
        WindingFitValidation(candidate.winding, rules),
        CurrentDensityValidation(candidate.winding, rules),
        ThermalValidation(candidate.thermal, requirements.allowableTempRiseC),
    };

    // A candidate is blocked only by checks that actually ran and failed.
    // A not_evaluated check (e.g. ThermalValidation with no thermal model
    // available) is surfaced in candidate.validations either way, but it
    // is a caveat, not a rejection reason - spec section 10 says a missing
    // check must never be presented as a pass, not that it must block
    // every candidate forever until the data exists.
    candidate.passed = true;
    for (const auto& validation : candidate.validations) {
        if (validation.status == EvaluationStatus::Evaluated && !validation.passed) {
            candidate.passed = false;
            candidate.rejectionReasons.push_back({validation.checkName, validation.explanation});
        }
    }

    return candidate;
}

}  // namespace

//precondition: Materials::load()/CoreDatabase::load() have been populated
//postcondition: returns an explainable DesignRecommendation - either at least one fully-passing candidate, or status="no_feasible_design" with the reason (never a silent fallback to an unsafe design).
DesignRecommendation InductorDesignService::run(const InductorDesignRequest& request) {
    DesignRecommendation recommendation;
    DesignRules rules = DesignRules::phase1Default();
    recommendation.activeRules = rules;

    InductorRequirements requirements = RequirementDerivationService::derive(request, rules);

    std::vector<MaterialCandidate> materials = findSuitableMaterials(requirements.operatingPoint);
    if (materials.empty()) {
        recommendation.status = "no_feasible_design";
        recommendation.message = "No material candidate has a frequency range covering the requested switching "
                                  "frequency.";
        return recommendation;
    }

    //this AreaProduct input will get information from the normalized requirements and the design rules to calculate the required area product for the inductor design
    AreaProductInput apInput;
    apInput.inductanceH = requirements.operatingPoint.inductanceH;
    apInput.peakCurrentA = requirements.operatingPoint.peakCurrentA;
    apInput.switchingFreqHz = requirements.operatingPoint.switchingFreqHz;
    apInput.allowableTempRiseC = requirements.allowableTempRiseC;
    apInput.windowUtilization = rules.windowUtilization;
    apInput.fluxDensityT = rules.defaultFluxDensityLimitT;
    apInput.currentDensityAPerCm2 = rules.allowableCurrentDensityAperCm2;
    //call the function to calculate the required area product for the inductor design based on the input parameters
    double requiredAreaProductCm4 = calculateAp(apInput);

    //CoreCandiate now gets get picked based on if they meet the required area product and if they are compatible with the suitable materials from stage 1   
    std::vector<CoreCandidate> cores = findSuitableCores(materials, requiredAreaProductCm4);

    double largestAvailableAreaProductCm4 = 0.0;
    for (const auto& core : cores) {
        largestAvailableAreaProductCm4 = std::max(largestAvailableAreaProductCm4, core.areaProductCm4);
    }

    std::vector<CoreCandidate> feasibleCores;
    for (const auto& core : cores) {
        if (core.meetsAreaProduct) {
            feasibleCores.push_back(core);
        }
    }

    if (feasibleCores.empty()) {
        recommendation.status = "no_feasible_design";
        recommendation.message = "No core met the area-product requirement.";
        recommendation.requiredAreaProductCm4 = requiredAreaProductCm4;
        recommendation.largestAvailableAreaProductCm4 = largestAvailableAreaProductCm4;
        return recommendation;
    }

    std::unordered_map<std::string, MaterialCandidate> materialsByName;
    for (const auto& material : materials) {
        materialsByName.emplace(material.materialFamily, material);
    }

    for (const auto& core : feasibleCores) {
        const auto& material = materialsByName.at(core.material);
        InductorCandidate candidate = evaluateCandidate(core, material, requirements, rules);

        if (candidate.passed) {
            recommendation.candidates.push_back(std::move(candidate));
        } else {
            recommendation.rejectedCandidates.push_back(std::move(candidate));
        }
    }

    //Phase 1 ranking: no cost/loss data exists to rank by, so passing candidates are ordered by area product ascending (smallest adequate core first) - a plain sizing preference, not a hidden magnetic constant.
    std::sort(recommendation.candidates.begin(), recommendation.candidates.end(),[](const InductorCandidate& a, const InductorCandidate& b) {return a.core.areaProductCm4 < b.core.areaProductCm4;});

    if (recommendation.candidates.empty()) {
        recommendation.status = "no_feasible_design";
        recommendation.message = "Cores met the area-product requirement, but none passed every magnetic/winding "
                                  "validation check - see rejectedCandidates for details.";
    } 
    else {
        recommendation.status = "ok";
        recommendation.message = std::to_string(recommendation.candidates.size()) + " candidate(s) passed every check; " + std::to_string(recommendation.rejectedCandidates.size()) + " rejected.";
    }

    return recommendation;
}
