#include "validation/RecommendationStatus.h"

//precondition: see header
//postcondition: see header
RecommendationClassification classifyRecommendation(bool passed, const std::vector<ValidationResult>& validations,
                                                      const SkinDepthRiskResult& acLossRisk) {
    RecommendationClassification result;

    bool anyPreliminary = false;
    for (const auto& v : validations) {
        if (v.status == EvaluationStatus::Evaluated) {
            result.checksEvaluatedCount++;
            if (v.passed) {
                result.checksPassedCount++;
            } else {
                result.checksFailedCount++;
            }
            if (v.isPreliminaryEstimate) {
                anyPreliminary = true;
            }
        } else {
            result.checksNotEvaluatedCount++;
            result.missingInfo.push_back(v.checkName + ": " + v.explanation);
        }
    }

    if (!passed) {
        result.tier = RecommendationTier::Rejected;
        result.explanation = "rejected: at least one evaluated check failed - see rejectionReasons";
        return result;
    }

    bool acRiskLimitsRecommendation = acLossRisk.riskLevel != AcLossRiskLevel::Low;
    bool anyNotEvaluated = result.checksNotEvaluatedCount > 0;

    if (anyNotEvaluated || anyPreliminary || acRiskLimitsRecommendation) {
        result.tier = RecommendationTier::PreliminaryCandidate;
        std::string reasons;
        if (anyNotEvaluated) {
            reasons += std::to_string(result.checksNotEvaluatedCount) + " check(s) not evaluated; ";
        }
        if (anyPreliminary) {
            reasons += "at least one check rests on a preliminary estimate, not measured data; ";
        }
        if (acRiskLimitsRecommendation) {
            reasons += std::string("AC-loss risk is ") + (acLossRisk.riskLevel == AcLossRiskLevel::High ? "High" : "Moderate") + "; ";
        }
        result.explanation = "preliminary candidate: passed every check that ran, but " + reasons;
        return result;
    }

    result.tier = RecommendationTier::Phase1Recommended;
    result.explanation = "recommended: every mandatory check evaluated and passed, no preliminary estimates, low AC-loss risk";
    return result;
}
