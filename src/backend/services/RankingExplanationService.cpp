#include "backend/services/RankingExplanationService.h"

namespace {
const char* tierName(RecommendationTier tier) {
    switch (tier) {
        case RecommendationTier::Pass:
            return "PASS";
        case RecommendationTier::ConditionalPass:
            return "CONDITIONAL_PASS";
        case RecommendationTier::Reject:
        default:
            return "REJECT";
    }
}
}  // namespace

//precondition: see header
//postcondition: see header
std::string explainRanking(const InductorCandidate& candidate) {
    return std::string("[") + tierName(candidate.recommendation.tier) + "] " + candidate.recommendation.explanation + " " +
           candidate.lossSummary.label + " (" + std::to_string(candidate.lossSummary.knownPartialLossW) +
           " W known partial loss, " + std::to_string(candidate.manufacturabilityMarginPercent) + "% manufacturability margin).";
}

//precondition: see header
//postcondition: see header
std::string suggestImprovement(const InductorCandidate& candidate) {
    const std::string& checkName = candidate.bottleneck.limitingCheckName;

    if (candidate.bottleneck.reason == BottleneckReason::NotEvaluatedCheck) {
        if (checkName == "SaturationValidation" || checkName == "PeakFluxValidation") {
            return "supply peak current to evaluate saturation risk";
        }
        if (checkName == "ThermalValidation") {
            return "thermal margin could not be evaluated for this candidate - the thermal model needs "
                   "known winding/loss geometry to run";
        }
        if (checkName == "BundleFitValidation") {
            return "bundle fit could not be evaluated - this core has no real window width/height on "
                   "record (always true for toroids), or the winding is single-strand and has no bundle "
                   "to check";
        }
        if (checkName == "CurrentConsistencyValidation") {
            return "supply both peak current and ripple current for a full current-waveform consistency "
                   "check (this check is diagnostic only and does not block the design either way)";
        }
        return "required data for " + checkName + " was not supplied - see its explanation for exactly "
               "what's missing";
    }

    if (candidate.bottleneck.reason == BottleneckReason::FailedCheck ||
        candidate.bottleneck.reason == BottleneckReason::MarginHeadroom) {
        if (checkName == "SaturationValidation" || checkName == "PeakFluxValidation") {
            return "saturation margin is the limiting factor - consider a larger core (more Ae), a "
                   "lower-permeability material, or more turns with a larger gap to lower peak flux "
                   "density";
        }
        if (checkName == "ThermalValidation") {
            return "temperature rise is the limiting factor - reduce copper loss (larger wire gauge, "
                   "fewer turns) or core loss (lower-loss material, lower ripple current), or improve "
                   "cooling";
        }
        if (checkName == "WindingFitValidation") {
            return "physical winding fill is the limiting factor - use a larger core, fewer turns, or a "
                   "smaller wire gauge to leave more room in the window";
        }
        if (checkName == "CurrentDensityValidation") {
            return "current density is the limiting factor - use a larger wire gauge, or add parallel "
                   "strands, to reduce it";
        }
        if (checkName == "BundleFitValidation") {
            return "the parallel-strand bundle is the limiting factor - use fewer parallel strands, a "
                   "smaller wire gauge, or a core with a wider window opening";
        }
        if (checkName == "InductanceValidation") {
            return "calculated inductance is the limiting factor - adjust turns, or use a core with a "
                   "different AL, to bring it back within tolerance";
        }
        if (checkName == "CurrentConsistencyValidation") {
            return "current-consistency is diagnostic only - it does not block this design either way";
        }
        return "the limiting factor was " + checkName + " - see its explanation for details";
    }

    return "no single limiting factor identified for this candidate";
}
