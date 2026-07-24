#include "backend/services/RankingExplanationService.h"

namespace {
const char* tierName(RecommendationTier tier) {
    switch (tier) {
        case RecommendationTier::Phase1Recommended:
            return "Phase1Recommended";
        case RecommendationTier::PreliminaryCandidate:
            return "PreliminaryCandidate";
        case RecommendationTier::Rejected:
        default:
            return "Rejected";
    }
}
}  // namespace

//precondition: see header
//postcondition: see header
std::string explainRanking(const InductorCandidate& candidate) {
    return std::string("[") + tierName(candidate.recommendation.tier) + "] " + candidate.recommendation.explanation + " " +
           candidate.lossSummary.label + " (" + std::to_string(candidate.lossSummary.knownEvaluatedLossW) +
           " W known evaluated loss, " + std::to_string(candidate.manufacturabilityMarginPercent) + "% manufacturability margin).";
}
