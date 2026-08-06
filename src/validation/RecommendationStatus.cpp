#include "validation/RecommendationStatus.h"


//precondition: validations is the candidate's full validation list (same one candidate.passed was derived from)
//postcondition: tier == Reject whenever passed == false (mirrors, never contradicts, the existing pass/fail decision - only checks flagged mandatory=true ever contribute to that decision upstream). Otherwise
//ConditionalPass when any mandatory check is NotEvaluated, any mandatory check carries isPreliminaryEstimate or usesDefaultAssumption, or acLossRisk.riskLevel is Moderate/High (acLossRisk.acLossWattsStatus is always
//NotEvaluated in Phase 1 too - see SkinDepthRisk.h). Pass only when none of those apply to any mandatory check - see the block comment above for why that is currently unreachable given this engine's real data
//coverage. This is the single centralized function that decides tier - no other code path may assign one.
RecommendationClassification determineRecommendationStatus(bool passed, const std::vector<ValidationResult>& validations, const SkinDepthRiskResult& acLossRisk) {
    RecommendationClassification result;

    bool anyMandatoryPreliminaryOrDefaulted = false;
    bool anyMandatoryNotEvaluated = false;
    //this will check all 7 mandatory validations check to see if they passed
    for (const auto& v : validations) {
        //if they were evaluated then go in here and keep track of what is being evaluated else go to not evaluated
        if (v.status == EvaluationStatus::Evaluated) {
            result.checksEvaluatedCount++;
            //if validation pass then increment the total by 1
            if (v.passed) {
                result.checksPassedCount++;
            } 
            else {
                result.checksFailedCount++;
            }
            if (v.mandatory && (v.isPreliminaryEstimate || v.usesDefaultAssumption)) {
                anyMandatoryPreliminaryOrDefaulted = true;
            }
        }
        else {
            result.checksNotEvaluatedCount++;
            result.missingInfo.push_back(v.checkName + ": " + v.explanation);
            if (v.mandatory) {
                anyMandatoryNotEvaluated = true;
            }
        }
    }
    //if none were pass, then error: rejected because one mandatory check did not pass
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
