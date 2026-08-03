#include "InductorDesignService.h"
#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include "core/sizing/AreaProduct.h"
#include "core/sizing/CoreEvaluation.h"
#include "core/losses/LossEvaluation.h"
#include "core/sizing/MaterialEvaluation.h"
#include "core/thermal/ThermalEvaluation.h"
#include "core/magnetics/TurnsAndGapDesign.h"
#include "core/winding/WindingDesign.h"
#include "core/losses/SkinDepthRisk.h"
#include "rules/DesignRules.h"
#include "validation/DesignValidation.h"
#include "validation/RecommendationStatus.h"
#include "RequirementDerivationService.h"
#include "core/units/UnitConversions.h"
#include "backend/services/RankingExplanationService.h"
#include "backend/services/CandidateRankingHelpers.h"
#include "backend/services/BottleneckAnalysisService.h"
#include "backend/services/RankingHighlightsService.h"
#include "core/sizing/FluxLimit.h"

//lets us reuse the function throughout the file without having to prefix it with the namespace
namespace {

//precondition: none
//postcondition: sum of copper + core loss for whichever of the two are Evaluated - never presented as a
//complete total, since AC-loss watts is never Evaluated in Phase 1 - see LossSummary.h.
LossSummary buildLossSummary(const LossEvaluationResult& losses) {
    LossSummary summary;
    //this will be used for the label in LossSummary
    std::string components;
    //if you have copper loss evaluated then add it to the known partial loss and also components
    if (losses.copperLossStatus == EvaluationStatus::Evaluated) {
        summary.knownPartialLossW += losses.copperLossW;
        components += "copper";
    }
    //if you have core loss evalauted then include that to get the sum of core loss with copper loss
    if (losses.coreLossStatus == EvaluationStatus::Evaluated) {
        summary.knownPartialLossW += losses.coreLossW;
        components += components.empty() ? "core" : "+core";
    }
    summary.isCompleteTotal = false;
    summary.label = components.empty() ? "Known Partial Loss: no loss components evaluated" : "Known Partial Loss (" + components + " - AC/skin-effect loss not modeled)";
    return summary;
}

InductorCandidate evaluateCandidate(const CoreCandidate& core, const MaterialCandidate& material, const InductorRequirements& requirements, const DesignRules& rules) {
    InductorCandidate candidate;
    candidate.material = material;
    candidate.core = core;
    candidate.fluxLimits = calculateFluxLimitTiers(material, rules);

    double targetInductanceUH = units::hToUH(requirements.operatingPoint.inductanceH);
    candidate.turnsAndGap = designTurnsAndGap(core, material, targetInductanceUH, requirements.inductanceTolerancePercent,
                                               requirements.operatingPoint.peakCurrentA, requirements.operatingPoint.rmsCurrentA, rules);

    if (!candidate.turnsAndGap.converged) {
        candidate.passed = false;
        candidate.recommendation.tier = RecommendationTier::Reject;
        candidate.recommendation.explanation = "rejected: turns/gap design did not converge - no further evaluation performed";
        for (const auto& reason : candidate.turnsAndGap.rejectionReasons) {
            candidate.rejectionReasons.push_back({"TurnsAndGapDesign", reason});
        }
        candidate.bottleneck = analyzeBottleneck(candidate);
        return candidate;
    }

    //Call order: winding (geometry + cold-reference DCR) -> losses (core loss, cold-reference copper loss)
    //-> thermal (the iterative loop, fed by both) -> overwrite the cold-reference hot-DCR/copper-loss
    //estimates with the converged result, but ONLY if the loop actually converged - otherwise the honest
    //cold-reference/sanity-check values computed above remain untouched -> skin-depth risk -> validations.
    candidate.winding = designWinding(core, candidate.turnsAndGap.turns, requirements.operatingPoint.rmsCurrentA, rules);

    candidate.losses = evaluateLosses(material, core, candidate.turnsAndGap, candidate.winding, requirements.operatingPoint.rmsCurrentA, requirements.operatingPoint.switchingFreqHz, requirements.operatingPoint.rippleCurrentPeakToPeakA);

    ThermalIterationInputs thermalInputs;
    thermalInputs.ambientTemperatureC = requirements.ambientTemperatureC;
    thermalInputs.rmsCurrentA = requirements.operatingPoint.rmsCurrentA;
    thermalInputs.coldDcrOhmsAt20C = candidate.winding.coldDcrOhmsAt20C;
    thermalInputs.copperLossGeometryKnown = candidate.winding.resistanceStatus == EvaluationStatus::Evaluated;
    thermalInputs.coreLossW = candidate.losses.coreLossW;
    thermalInputs.coreLossKnown = candidate.losses.coreLossStatus == EvaluationStatus::Evaluated;
    thermalInputs.coreEffectiveAreaMm2 = core.aeMm2;
    thermalInputs.coreMagneticPathLengthMm = core.leMm;
    candidate.thermal = evaluateThermal(thermalInputs, rules);

    if (candidate.thermal.status == ThermalStatus::PreliminaryThermalEstimate) {
        candidate.winding.estimatedHotDcrOhms = candidate.thermal.hotDcrOhms;
        if (candidate.losses.copperLossStatus == EvaluationStatus::Evaluated) {
            candidate.losses.copperLossW = candidate.thermal.copperLossAtConvergedTempW;
        }
    }

    candidate.acLossRisk = evaluateSkinDepthRisk(requirements.operatingPoint.switchingFreqHz, candidate.winding.conductorAreaMm2, rules);

    candidate.validations = {
        CurrentConsistencyValidation(requirements.operatingPoint),
        InductanceValidation(candidate.turnsAndGap, requirements.inductanceTolerancePercent),
        PeakFluxValidation(core, material, candidate.turnsAndGap, requirements.operatingPoint.peakCurrentA, requirements.operatingPoint.rmsCurrentA, rules),
        SaturationValidation(core, material, candidate.turnsAndGap, requirements.operatingPoint.peakCurrentA, requirements.operatingPoint.rmsCurrentA, rules),
        WindingFitValidation(candidate.winding, rules),
        CurrentDensityValidation(candidate.winding, rules),
        BundleFitValidation(candidate.winding),
        ThermalValidation(candidate.thermal, requirements.allowableTempRiseC),
    };

    //A candidate is blocked only by checks that actually ran and failed.
    //A not_evaluated check (e.g. ThermalValidation with no thermal model
    //available) is surfaced in candidate.validations either way, but it
    //is a warning, not a rejection reason - spec section 10 says a missing
    //check must never be presented as a pass, not that it must block
    //every candidate forever until the data exists.
    candidate.passed = true;
    for (const auto& validation : candidate.validations) {
        if (validation.status == EvaluationStatus::Evaluated && !validation.passed) {
            candidate.passed = false;
            candidate.rejectionReasons.push_back({validation.checkName, validation.explanation});
        }
    }

    candidate.recommendation = determineRecommendationStatus(candidate.passed, candidate.validations, candidate.acLossRisk);
    candidate.lossSummary = buildLossSummary(candidate.losses);

    //Manufacturability margin (spec section 11): a documented simple composite of the two concrete
    //manufacturability signals this pipeline actually produces - physical-fill headroom against
    //rules.maximumFillFactor, and a fixed penalty when the calculated gap triggered the
    //small-gap manufacturability warning (TurnsAndGapDesign.h). Not a validated single-number metric.
    double fillHeadroomPercent = rules.maximumFillFactor > 0.0 ? 100.0 * (rules.maximumFillFactor - candidate.winding.physicalWindowFillFactor) / rules.maximumFillFactor : 0.0;
    double smallGapPenaltyPercent = candidate.turnsAndGap.smallGapWarning ? 25.0 : 0.0;
    candidate.manufacturabilityMarginPercent = fillHeadroomPercent - smallGapPenaltyPercent;

    candidate.bottleneck = analyzeBottleneck(candidate);
    candidate.rankingExplanation = explainRanking(candidate);
    candidate.designNarrative = suggestImprovement(candidate);

    return candidate;
}

//precondition: recommendation.candidates only ever contains passed==true candidates (see run()'s split below),
//so tier here is always Pass or ConditionalPass, never Reject - the tier comparison is still real and
//future-proofed, it simply currently has nothing to differentiate (Pass is unreachable today - see
//RecommendationStatus.h) and falls straight through to the finer-grained tiebreakers, which is where real
//differentiation actually happens in the current dataset.
//postcondition: true if a should rank strictly ahead of b
bool candidateRanksAhead(const InductorCandidate& a, const InductorCandidate& b) {
    if (a.recommendation.tier != b.recommendation.tier) {
        return static_cast<int>(a.recommendation.tier) < static_cast<int>(b.recommendation.tier);
    }
    if (a.lossSummary.knownPartialLossW != b.lossSummary.knownPartialLossW) {
        return a.lossSummary.knownPartialLossW < b.lossSummary.knownPartialLossW;
    }
    double riseA = thermalRiseForRanking(a);
    double riseB = thermalRiseForRanking(b);
    if (riseA != riseB) {
        return riseA < riseB;
    }
    if (a.manufacturabilityMarginPercent != b.manufacturabilityMarginPercent) {
        return a.manufacturabilityMarginPercent > b.manufacturabilityMarginPercent;
    }
    double satA = marginForRanking(a, "SaturationValidation", true);
    double satB = marginForRanking(b, "SaturationValidation", true);
    if (satA != satB) {
        return satA > satB;
    }
    double cdA = marginForRanking(a, "CurrentDensityValidation", false);
    double cdB = marginForRanking(b, "CurrentDensityValidation", false);
    if (cdA != cdB) {
        return cdA > cdB;
    }
    if (a.core.areaProductCm4 != b.core.areaProductCm4) {
        return a.core.areaProductCm4 < b.core.areaProductCm4;
    }
     //deterministic final tiebreak
    return a.core.partNumber < b.core.partNumber; 
}

}  //namespace

//precondition: Materials::load()/CoreDatabase::load() have been populated
//postcondition: returns an explainable DesignRecommendation - either at least one fully-passing candidate, or status="no_feasible_design" with the reason (never a silent fallback to an unsafe design).
DesignRecommendation InductorDesignService::run(const InductorDesignRequest& request) {
    DesignRecommendation recommendation;
    DesignRules rules = DesignRules::phase1Default();
    recommendation.activeRules = rules;
    recommendation.versions = EngineVersionsStore::current();

    //what the inductor MUST have, so it goes to the derivation to get the correct information
    InductorRequirements requirements = RequirementDerivationService::derive(request, rules);

    //this will loop through the material database and grab the materials that are within the range of the frequency
    std::vector<MaterialCandidate> materials = findSuitableMaterials(requirements.operatingPoint);
    if (materials.empty()) {
        recommendation.status = "no_feasible_design";
        recommendation.message = "No material candidate has a frequency range covering the requested switching frequency.";
        return recommendation;
    }

    //Area-Product pre-filter needs a real peak current (Ap's stored-energy formula is 0.5*L*Ipk^2) - when
    //peak wasn't supplied, there is no honest number to feed it, and substituting RMS in Ipk's place would
    //understate the real peak and silently undersize the core, which this engine never does. In that case
    //every frequency/material-compatible core is evaluated directly instead (requiredAreaProductCm4 of 0.0
    //makes findSuitableCores() flag every core as meeting it, which is the correct "no pre-filter" result -
    //PeakFluxValidation/SaturationValidation report the real gap per candidate instead).
    bool peakSupplied = requirements.operatingPoint.peakCurrentA.has_value();
    //computed PER Material, not once for the whole request - Ap is inversely proportional to the flux density
    //limit used (see AreaProduct.cpp) and that limit is genuinely different per material (a ferrite-typical Bmax vs
    //a powder core real, usually much higher, Bsat from real_materials.csv). Using one flat default here for every material
    //silently under-credited high-Bsat pwoder cores (MPP, Kool Mu, XFlux, etc) - they need real_materials.csv own applicableFluxLimit()
    //to size correctly, but were being judged against a ferrite-typical number instead, dropping otherwise-viable candidates before they ever
    //reached the real per-candidate SturationValidation/PeakFluxValidation checks
    std::unordered_map<std::string, double> requiredAreaProductCm4ByMaterial;
    for(const auto& material: materials){
        double requiredAreaProductCm4 = 0.0;
        //if there is a peak current then do the Ap else do not do it and skip the filter
        if (peakSupplied) {
            AreaProductInput apInput;
            apInput.inductanceH = requirements.operatingPoint.inductanceH;
            apInput.peakCurrentA = *requirements.operatingPoint.peakCurrentA;
            apInput.switchingFreqHz = requirements.operatingPoint.switchingFreqHz;
            apInput.allowableTempRiseC = requirements.allowableTempRiseC;
            apInput.windowUtilization = rules.windowUtilization;
            apInput.fluxDensityT = applicableFluxLimit(material, rules).limitT;
            apInput.currentDensityAPerCm2 = rules.allowableCurrentDensityAperCm2;
            //call the function to calculate the required area product for the inductor design based on the input parameters
            requiredAreaProductCm4 = calculateAp(apInput);
        }
        //call the key and set the value equal to that which is required Ap for that specific material
        requiredAreaProductCm4ByMaterial[material.materialFamily] = requiredAreaProductCm4;
    }

    //smallest requirement across all compatible materials - the easiest one to satisfy - not any single materials own number
    //since there is no longer one "the" requried Ap for the whole request. Used only for the no_feasible_design report below: 
    //"even the easiest material requirment wasnt met" is an honest, non-misleading single number to show, unlike averaging or picking an arbitrary material
    double smallestRequiredAreaProductCm4 = 0.0;
    if(!requiredAreaProductCm4ByMaterial.empty()) {
        //will start at the beginning of the unordered map
        smallestRequiredAreaProductCm4 = requiredAreaProductCm4ByMaterial.begin()->second;
        for(const auto& materialRequiremnet : requiredAreaProductCm4ByMaterial) {
            if(materialRequiremnet.second < smallestRequiredAreaProductCm4) {
                //if small then now update it so we can recheck the others to the new small number
                smallestRequiredAreaProductCm4 = materialRequiremnet.second;
            }
        }

    }


    //CoreCandiate now gets get picked based on if they meet the required area product and if they are compatible with the suitable materials from stage 1
    std::vector<CoreCandidate> cores = findSuitableCores(materials, requiredAreaProductCm4ByMaterial);

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

    //if there is no core that has a large enough area product to meet the requirements, return a no feasible design recommendation with the required and largest available area product values.
    //When peak wasn't supplied, requiredAreaProductCm4 is 0.0 and every material-compatible core unimportant
    //meets it - this branch is then only reachable if there were zero material-compatible cores at all,
    //which findSuitableCores() would only produce for a materials list with no matching cores in the DB.
    if (feasibleCores.empty()) {
        recommendation.status = "no_feasible_design";
        recommendation.message = peakSupplied
            ? "No core met the area-product requirement."
            : "No core found for the compatible materials at this frequency (no peak current was supplied, so no area-product pre-filter was applied - this means the database itself has no matching core, not that one was filtered out).";
        recommendation.requiredAreaProductCm4 = smallestRequiredAreaProductCm4;
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
        } 
        else {
            recommendation.rejectedCandidates.push_back(std::move(candidate));
        }
    }

    //Optimization layer (spec sections 10-11): rank by 3-tier recommendation classification first (see
    //RecommendationStatus.h), then within a tier by known evaluated loss, predicted temperature rise,
    //manufacturability margin, saturation margin, current-density margin, area product, and finally part
    //number as a deterministic final tiebreak - see candidateRanksAhead() above for the full comparator.
    std::stable_sort(recommendation.candidates.begin(), recommendation.candidates.end(), candidateRanksAhead);
    recommendation.rankingHighlights = computeRankingHighlights(recommendation.candidates);

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
