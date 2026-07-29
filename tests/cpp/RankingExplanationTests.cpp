#include <cassert>
#include <cstdio>
#include "backend/services/RankingExplanationService.h"

namespace {

//1. Baseline regression: explainRanking() must still combine tier + explanation + loss + margin into
//one non-empty sentence (unchanged behavior - this file had zero prior test coverage).
void testExplainRankingBaselineShape() {
    InductorCandidate candidate;
    candidate.recommendation.tier = RecommendationTier::ConditionalPass;
    candidate.recommendation.explanation = "conditional pass: test explanation";
    candidate.lossSummary.label = "Known Partial Loss (copper+core - AC/skin-effect loss not modeled)";
    candidate.lossSummary.knownPartialLossW = 1.5;
    candidate.manufacturabilityMarginPercent = 42.0;

    std::string result = explainRanking(candidate);

    assert(!result.empty());
    assert(result.find("CONDITIONAL_PASS") != std::string::npos);
    assert(result.find("conditional pass: test explanation") != std::string::npos);
    std::printf("testExplainRankingBaselineShape: %s\n", result.c_str());
}

//2. suggestImprovement(): NotEvaluatedCheck + SaturationValidation must return the EXACT required
//phrase, never a generic guess.
void testSuggestImprovementExactPhraseForMissingPeakCurrent() {
    InductorCandidate candidate;
    candidate.bottleneck.reason = BottleneckReason::NotEvaluatedCheck;
    candidate.bottleneck.limitingCheckName = "SaturationValidation";

    std::string result = suggestImprovement(candidate);

    assert(result == "supply peak current to evaluate saturation risk");
    std::printf("testSuggestImprovementExactPhraseForMissingPeakCurrent: %s\n", result.c_str());
}

//3. Same exact phrase for PeakFluxValidation NotEvaluated (the other check gated on peak current).
void testSuggestImprovementExactPhraseForPeakFluxNotEvaluated() {
    InductorCandidate candidate;
    candidate.bottleneck.reason = BottleneckReason::NotEvaluatedCheck;
    candidate.bottleneck.limitingCheckName = "PeakFluxValidation";

    std::string result = suggestImprovement(candidate);

    assert(result == "supply peak current to evaluate saturation risk");
    std::printf("testSuggestImprovementExactPhraseForPeakFluxNotEvaluated: %s\n", result.c_str());
}

//4. MarginHeadroom + ThermalValidation must return a distinct, engineering-grounded suggestion (not
//the same text as the saturation case).
void testSuggestImprovementThermalMarginHeadroom() {
    InductorCandidate candidate;
    candidate.bottleneck.reason = BottleneckReason::MarginHeadroom;
    candidate.bottleneck.limitingCheckName = "ThermalValidation";

    std::string result = suggestImprovement(candidate);

    assert(!result.empty());
    assert(result != "supply peak current to evaluate saturation risk");
    std::printf("testSuggestImprovementThermalMarginHeadroom: %s\n", result.c_str());
}

//5. Fallback: an unmapped/unknown check name must still return an honest, non-empty string.
void testSuggestImprovementFallbackNeverEmpty() {
    InductorCandidate candidate;
    candidate.bottleneck.reason = BottleneckReason::FailedCheck;
    candidate.bottleneck.limitingCheckName = "SomeFutureCheckNotYetMapped";

    std::string result = suggestImprovement(candidate);

    assert(!result.empty());
    std::printf("testSuggestImprovementFallbackNeverEmpty: %s\n", result.c_str());
}

//6. reason == None must also return a non-empty, honest string, never silently empty.
void testSuggestImprovementNoneReasonNeverEmpty() {
    InductorCandidate candidate;
    candidate.bottleneck.reason = BottleneckReason::None;

    std::string result = suggestImprovement(candidate);

    assert(!result.empty());
    std::printf("testSuggestImprovementNoneReasonNeverEmpty: %s\n", result.c_str());
}

}  // namespace

void runRankingExplanationTests() {
    testExplainRankingBaselineShape();
    testSuggestImprovementExactPhraseForMissingPeakCurrent();
    testSuggestImprovementExactPhraseForPeakFluxNotEvaluated();
    testSuggestImprovementThermalMarginHeadroom();
    testSuggestImprovementFallbackNeverEmpty();
    testSuggestImprovementNoneReasonNeverEmpty();
    std::printf("All RankingExplanationTests passed.\n");
}
