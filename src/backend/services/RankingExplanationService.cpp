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
