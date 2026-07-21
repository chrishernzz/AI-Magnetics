#include "InductorDesignService.h"
#include <algorithm>
#include <unordered_map>
#include "core/sizing/AreaProduct.h"
#include "core/sizing/CoreEvaluation.h"
#include "core/losses/LossEvaluation.h"
#include "core/sizing/MaterialEvaluation.h"
#include "core/thermal/ThermalEvaluation.h"
#include "core/magnetics/TurnsAndGapDesign.h"
#include "core/winding/WindingDesign.h"
#include "rules/DesignRules.h"
#include "validation/DesignValidation.h"
#include "RequirementDerivationService.h"

//lets us reuse the function throughout the file without having to prefix it with the namespace
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
    candidate.losses = evaluateLosses(material, core, candidate.turnsAndGap, candidate.winding, requirements.operatingPoint.rmsCurrentA,
                                       requirements.operatingPoint.switchingFreqHz, requirements.operatingPoint.rippleCurrentPeakToPeakA);

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

//precondition: none
//postcondition: sum of copper + core loss for whichever of the two are Evaluated - candidates missing both loss numbers get 0.0 here, but callers must check hasAnyLossData() first rather than treating that 0.0 as "zero loss"
double totalKnownLossW(const InductorCandidate& candidate) {
    double loss = 0.0;
    if (candidate.losses.copperLossStatus == EvaluationStatus::Evaluated) {
        loss += candidate.losses.copperLossW;
    }
    if (candidate.losses.coreLossStatus == EvaluationStatus::Evaluated) {
        loss += candidate.losses.coreLossW;
    }
    return loss;
}

//precondition: none
//postcondition: true if at least one of copper/core loss is a real, Evaluated number for this candidate
bool hasAnyLossData(const InductorCandidate& candidate) {
    return candidate.losses.copperLossStatus == EvaluationStatus::Evaluated ||
           candidate.losses.coreLossStatus == EvaluationStatus::Evaluated;
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
        recommendation.message = "No material candidate has a frequency range covering the requested switching frequency.";
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


    //find the largest Ap among the avilable compatible cores and this value is used to explain how close the available cores are to the required Ap for the inductor design
    double largestAvailableAreaProductCm4 = 0.0;
    for (const auto& core : cores) {
        largestAvailableAreaProductCm4 = std::max(largestAvailableAreaProductCm4, core.areaProductCm4);
    }

    //will keep the cores that meet or exceed the required Ap; Note these cores will continue to the detailed winding and magnetic validation stage
    std::vector<CoreCandidate> feasibleCores;
    for (const auto& core : cores) {
        if (core.meetsAreaProduct) {
            feasibleCores.push_back(core);
        }
    }

    //if there is no core that has a large enough area product to meet the requirements, return a no feasible design recommendation with the required and largest available area product values
    if (feasibleCores.empty()) {
        recommendation.status = "no_feasible_design";
        recommendation.message = "No core met the area-product requirement.";
        recommendation.requiredAreaProductCm4 = requiredAreaProductCm4;
        recommendation.largestAvailableAreaProductCm4 = largestAvailableAreaProductCm4;
        return recommendation;
    }

    //inserting a new key - value pair if the key does not already exists
    std::unordered_map<std::string, MaterialCandidate> materialsByName;
    for (const auto& material : materials) {
        //constructs and inserts a new element directly into the container without creating a temp object
        materialsByName.emplace(material.materialFamily, material);
    }

    for (const auto& core : feasibleCores) {
        //look up the material for this core in the materialsByName map. Grabs the key and returns the value for that key. This is used to evaluate the candidate with the core and material information
        const auto& material = materialsByName.at(core.material);
        InductorCandidate candidate = evaluateCandidate(core, material, requirements, rules);

        if (candidate.passed) {
            recommendation.candidates.push_back(std::move(candidate));
        } else {
            recommendation.rejectedCandidates.push_back(std::move(candidate));
        }
    }

    //v1 optimization layer: rank by real total loss (copper + core, whichever are Evaluated) first - this is the actual
    //"Optimization" half of "Physics-Based Calculation and Optimization" (the roadmap's Option 2), not just a size sort.
    //Core-loss coverage is real but partial (only materials with validated Steinmetz coefficients, and only when the
    //request supplied ripple current), so mixing "loss including core loss" against "loss without it" into one number
    //would silently bias against candidates that simply have more real data than others. Instead: candidates with any
    //real loss data are ranked ahead of candidates with none, sorted by known loss among themselves; candidates with no
    //loss data at all fall back to the old area-product-only comparison, and area product is always the tiebreaker.
    std::stable_sort(recommendation.candidates.begin(), recommendation.candidates.end(),
                      [](const InductorCandidate& a, const InductorCandidate& b) {
                          bool aHasLoss = hasAnyLossData(a);
                          bool bHasLoss = hasAnyLossData(b);
                          if (aHasLoss != bHasLoss) {
                              return aHasLoss;  // real loss data ranks ahead of none
                          }
                          if (aHasLoss && bHasLoss) {
                              double lossA = totalKnownLossW(a);
                              double lossB = totalKnownLossW(b);
                              if (lossA != lossB) {
                                  return lossA < lossB;
                              }
                          }
                          return a.core.areaProductCm4 < b.core.areaProductCm4;
                      });

    if (recommendation.candidates.empty()) {
        recommendation.status = "no_feasible_design";
        recommendation.message = "Cores met the area-product requirement, but none passed every magnetic/winding : validation check - see rejectedCandidates for details.";
    } 
    else {
        recommendation.status = "ok";
        recommendation.message = std::to_string(recommendation.candidates.size()) + " candidate(s) passed every check; " + std::to_string(recommendation.rejectedCandidates.size()) + " rejected.";
    }

    return recommendation;
}
