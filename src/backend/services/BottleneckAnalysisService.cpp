#include "backend/services/BottleneckAnalysisService.h"
#include "backend/services/CandidateRankingHelpers.h"
#include <array>
#include <limits>

namespace {

//SaturationValidation's calculatedValue is already a signed margin percent (see
//DesignValidation.cpp::SaturationValidation) - every other mandatory check's margin is limitValue -
//calculatedValue. This mirrors the exact actualIsTheMargin convention candidateRanksAhead() already
//uses when calling marginForRanking() for SaturationValidation/CurrentDensityValidation.
bool actualIsTheMargin(const std::string& checkName) {
    return checkName == "SaturationValidation";
}

//CurrentConsistencyValidation has a placeholder limitValue=0.0 and is structurally unable to fail
//(see DesignValidation.cpp) - it's diagnostic, not a physical gate, and is excluded from every
//margin comparison below.
bool excludedFromMarginComparison(const std::string& checkName) {
    return checkName == "CurrentConsistencyValidation";
}

//precondition: v.status == EvaluationStatus::Evaluated
//postcondition: dimensionless "fraction of budget remaining" margin (rawMargin / limitValue), or the
//raw margin itself when limitValue is 0 (defensive - avoids a division by zero on a degenerate
//user-supplied limit like allowableTempRiseC=0; not expected in practice).
double normalizedMargin(const InductorCandidate& candidate, const std::string& checkName) {
    double raw = marginForRanking(candidate, checkName.c_str(), actualIsTheMargin(checkName));
    const ValidationResult* v = findValidation(candidate, checkName.c_str());
    if (v == nullptr || v->limitValue == 0.0) {
        return raw;
    }
    return raw / v->limitValue;
}

//Priority order (highest first) for which mandatory NotEvaluated check to report when more than one
//is simultaneously not evaluated - without this, BundleFitValidation (NotEvaluated for almost every
//single-strand winding) would drown out the far more engineering-significant "peak current was never
//supplied" case. Safety/physical-risk checks outrank manufacturability/diagnostic ones.
const std::array<const char*, 8> kNotEvaluatedPriority = {
    "SaturationValidation", "PeakFluxValidation",     "ThermalValidation",     "BundleFitValidation",
    "CurrentConsistencyValidation", "InductanceValidation", "WindingFitValidation", "CurrentDensityValidation",
};

}  //namespace

//precondition: see header
//postcondition: see header
BottleneckAnalysis analyzeBottleneck(const InductorCandidate& candidate) {
    BottleneckAnalysis result;

    //Turns/gap never converged - evaluateCandidate() returns before any ValidationResult exists at all
    //(see InductorDesignService.cpp). The real limiting factor is whatever TurnsAndGapDesign reported.
    if (candidate.validations.empty()) {
        if (!candidate.rejectionReasons.empty()) {
            result.reason = BottleneckReason::FailedCheck;
            result.limitingCheckName = candidate.rejectionReasons.front().checkName;
            result.explanation = candidate.rejectionReasons.front().explanation;
        }
        return result;
    }

    if (!candidate.passed) {
        //Among the checks that actually failed, the one closest to passing (least-negative normalized
        //margin) is the real bottleneck - the other failures are also real, but this is the one that
        //would flip first with the smallest design change.
        bool found = false;
        double bestMargin = -std::numeric_limits<double>::max();
        for (const auto& rejection : candidate.rejectionReasons) {
            if (excludedFromMarginComparison(rejection.checkName)) {
                continue;
            }
            double margin = normalizedMargin(candidate, rejection.checkName);
            if (!found || margin > bestMargin) {
                found = true;
                bestMargin = margin;
                result.limitingCheckName = rejection.checkName;
                result.explanation = rejection.explanation;
            }
        }
        if (found) {
            result.reason = BottleneckReason::FailedCheck;
            result.marginValue = bestMargin;
            result.marginValueApplicable = true;
        } else if (!candidate.rejectionReasons.empty()) {
            //every rejection reason was CurrentConsistencyValidation (should not happen in practice,
            //since that check cannot fail - defensive fallback, never silently empty).
            result.reason = BottleneckReason::FailedCheck;
            result.limitingCheckName = candidate.rejectionReasons.front().checkName;
            result.explanation = candidate.rejectionReasons.front().explanation;
        }
        return result;
    }

    //Passing (Pass or ConditionalPass tier). A NotEvaluated mandatory check is a data-completeness gap
    //and always takes priority over reporting margin headroom - "we don't know" outranks "it has margin
    //on what we could check."
    for (const char* checkName : kNotEvaluatedPriority) {
        const ValidationResult* v = findValidation(candidate, checkName);
        if (v != nullptr && v->mandatory && v->status != EvaluationStatus::Evaluated) {
            result.reason = BottleneckReason::NotEvaluatedCheck;
            result.limitingCheckName = checkName;
            result.explanation = v->explanation;
            return result;
        }
    }
    //Any mandatory NotEvaluated check not covered by the priority list above (should not happen given
    //the 8 checks this engine runs today, but defensive against a future check being added without
    //updating the list).
    for (const auto& v : candidate.validations) {
        if (v.mandatory && v.status != EvaluationStatus::Evaluated) {
            result.reason = BottleneckReason::NotEvaluatedCheck;
            result.limitingCheckName = v.checkName;
            result.explanation = v.explanation;
            return result;
        }
    }

    //Every mandatory check ran and passed - the bottleneck is whichever evaluated check has the
    //smallest normalized margin, i.e. would fail first if conditions worsened slightly.
    bool found = false;
    double worstMargin = std::numeric_limits<double>::max();
    for (const auto& v : candidate.validations) {
        if (v.status != EvaluationStatus::Evaluated || excludedFromMarginComparison(v.checkName)) {
            continue;
        }
        double margin = normalizedMargin(candidate, v.checkName);
        if (!found || margin < worstMargin) {
            found = true;
            worstMargin = margin;
            result.limitingCheckName = v.checkName;
            result.explanation = v.explanation;
        }
    }
    if (found) {
        result.reason = BottleneckReason::MarginHeadroom;
        result.marginValue = worstMargin;
        result.marginValueApplicable = true;
    }

    return result;
}
