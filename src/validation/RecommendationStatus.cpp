#include "validation/RecommendationStatus.h"

//precondition: see header
//postcondition: see header
RecommendationClassification determineRecommendationStatus(bool passed, const std::vector<ValidationResult>& validations,
                                                             const SkinDepthRiskResult& acLossRisk) {
    RecommendationClassification result;

    bool anyMandatoryPreliminaryOrDefaulted = false;
    bool anyMandatoryNotEvaluated = false;
    for (const auto& v : validations) {
        if (v.status == EvaluationStatus::Evaluated) {
            result.checksEvaluatedCount++;
            if (v.passed) {
                result.checksPassedCount++;
            } else {
                result.checksFailedCount++;
            }
            if (v.mandatory && (v.isPreliminaryEstimate || v.usesDefaultAssumption)) {
                anyMandatoryPreliminaryOrDefaulted = true;
            }
        } else {
            result.checksNotEvaluatedCount++;
            result.missingInfo.push_back(v.checkName + ": " + v.explanation);
            if (v.mandatory) {
                anyMandatoryNotEvaluated = true;
            }
        }
    }

    if (!passed) {
        result.tier = RecommendationTier::Reject;
        result.explanation = "rejected: at least one mandatory check was evaluated and failed - see rejectionReasons";
        return result;
    }

    bool acRiskLimitsRecommendation = acLossRisk.riskLevel != AcLossRiskLevel::Low;

    if (anyMandatoryNotEvaluated || anyMandatoryPreliminaryOrDefaulted || acRiskLimitsRecommendation) {
        result.tier = RecommendationTier::ConditionalPass;
        std::string reasons;
        if (anyMandatoryNotEvaluated) {
            reasons += std::to_string(result.checksNotEvaluatedCount) + " mandatory check(s) not evaluated; ";
        }
        if (anyMandatoryPreliminaryOrDefaulted) {
            reasons += "at least one mandatory check rests on a Phase 1 default assumption or preliminary estimate, not measured data; ";
        }
        if (acRiskLimitsRecommendation) {
            reasons += std::string("AC-loss risk is ") + (acLossRisk.riskLevel == AcLossRiskLevel::High ? "High" : "Moderate") + "; ";
        }
        result.explanation = "conditional pass: every mandatory check that ran, passed, but " + reasons;
        return result;
    }

    result.tier = RecommendationTier::Pass;
    result.explanation = "pass: every mandatory check was evaluated and passed, every value is measured (not a Phase 1 default assumption or preliminary estimate), low AC-loss risk";
    return result;
}
