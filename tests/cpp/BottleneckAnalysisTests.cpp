#include <cassert>
#include <cstdio>
#include "TestHelpers.h"
#include "backend/services/BottleneckAnalysisService.h"

namespace {

ValidationResult makeValidation(const std::string& checkName, EvaluationStatus status, bool passed,
                                 double calculatedValue, double limitValue, bool mandatory = true) {
    ValidationResult v;
    v.checkName = checkName;
    v.status = status;
    v.passed = passed;
    v.calculatedValue = calculatedValue;
    v.limitValue = limitValue;
    v.mandatory = mandatory;
    v.explanation = checkName + " test explanation";
    return v;
}

//1. Rejected candidate with exactly one failed mandatory check - that's the bottleneck.
void testRejectedWithOneFailure() {
    InductorCandidate candidate;
    candidate.passed = false;
    candidate.validations = {
        makeValidation("InductanceValidation", EvaluationStatus::Evaluated, true, 1.0, 10.0),
        makeValidation("CurrentDensityValidation", EvaluationStatus::Evaluated, false, 5.0, 4.0),
    };
    candidate.rejectionReasons = {{"CurrentDensityValidation", "over the allowable current density"}};

    BottleneckAnalysis result = analyzeBottleneck(candidate);

    assert(result.reason == BottleneckReason::FailedCheck);
    assert(result.limitingCheckName == "CurrentDensityValidation");
    assert(result.marginValueApplicable);
    std::printf("testRejectedWithOneFailure: limitingCheckName=%s\n", result.limitingCheckName.c_str());
}

//2. Rejected candidate with two failed mandatory checks - the bottleneck must be whichever is closest
//to passing (least-negative normalized margin), not just the first one in the list.
void testRejectedWithMultipleFailuresPicksClosestToPassing() {
    InductorCandidate candidate;
    candidate.passed = false;
    //CurrentDensityValidation: margin = 4.0-5.0 = -1.0, normalized = -1.0/4.0 = -0.25 (far from passing)
    //WindingFitValidation: margin = 0.90-0.95 = -0.05, normalized = -0.05/0.90 = -0.0556 (close to passing)
    candidate.validations = {
        makeValidation("CurrentDensityValidation", EvaluationStatus::Evaluated, false, 5.0, 4.0),
        makeValidation("WindingFitValidation", EvaluationStatus::Evaluated, false, 0.95, 0.90),
    };
    candidate.rejectionReasons = {
        {"CurrentDensityValidation", "over the allowable current density"},
        {"WindingFitValidation", "physical fill exceeds maximum"},
    };

    BottleneckAnalysis result = analyzeBottleneck(candidate);

    assert(result.reason == BottleneckReason::FailedCheck);
    assert(result.limitingCheckName == "WindingFitValidation");
    std::printf("testRejectedWithMultipleFailuresPicksClosestToPassing: limitingCheckName=%s (margin=%.4f)\n",
                result.limitingCheckName.c_str(), result.marginValue);
}

//3. Turns/gap never converged - candidate.validations is empty (evaluateCandidate()'s early return).
//The fallback must use rejectionReasons directly, never crash on an empty validations list.
void testEarlyReturnNonConvergenceFallback() {
    InductorCandidate candidate;
    candidate.passed = false;
    candidate.rejectionReasons = {{"TurnsAndGapDesign", "required gap exceeds the practical bound"}};
    // candidate.validations left empty

    BottleneckAnalysis result = analyzeBottleneck(candidate);

    assert(result.reason == BottleneckReason::FailedCheck);
    assert(result.limitingCheckName == "TurnsAndGapDesign");
    assert(!result.marginValueApplicable);
    std::printf("testEarlyReturnNonConvergenceFallback: limitingCheckName=%s\n", result.limitingCheckName.c_str());
}

//4. Passing candidate with SaturationValidation NotEvaluated (no peak current supplied) - this must be
//reported as the bottleneck, not silently skipped in favor of margin headroom on the other checks.
void testPassingWithSaturationNotEvaluated() {
    InductorCandidate candidate;
    candidate.passed = true;
    candidate.validations = {
        makeValidation("InductanceValidation", EvaluationStatus::Evaluated, true, 1.0, 10.0),
        makeValidation("SaturationValidation", EvaluationStatus::NotEvaluated, false, 0.0, 10.0),
        makeValidation("WindingFitValidation", EvaluationStatus::Evaluated, true, 0.30, 0.90),
    };

    BottleneckAnalysis result = analyzeBottleneck(candidate);

    assert(result.reason == BottleneckReason::NotEvaluatedCheck);
    assert(result.limitingCheckName == "SaturationValidation");
    std::printf("testPassingWithSaturationNotEvaluated: limitingCheckName=%s\n", result.limitingCheckName.c_str());
}

//5. Passing candidate with BOTH BundleFitValidation and SaturationValidation NotEvaluated - priority
//order must pick SaturationValidation (safety-relevant), not BundleFitValidation (manufacturability),
//even though BundleFit appears earlier in a typical validations list.
void testPriorityPicksSaturationOverBundleFit() {
    InductorCandidate candidate;
    candidate.passed = true;
    candidate.validations = {
        makeValidation("BundleFitValidation", EvaluationStatus::NotEvaluated, false, 0.0, 0.0),
        makeValidation("SaturationValidation", EvaluationStatus::NotEvaluated, false, 0.0, 10.0),
        makeValidation("InductanceValidation", EvaluationStatus::Evaluated, true, 1.0, 10.0),
    };

    BottleneckAnalysis result = analyzeBottleneck(candidate);

    assert(result.reason == BottleneckReason::NotEvaluatedCheck);
    assert(result.limitingCheckName == "SaturationValidation");
    std::printf("testPriorityPicksSaturationOverBundleFit: limitingCheckName=%s (priority correctly favored Saturation)\n",
                result.limitingCheckName.c_str());
}

//6. Passing candidate, every mandatory check Evaluated - bottleneck is the smallest normalized margin,
//not the first check in the list.
void testPassingAllEvaluatedPicksSmallestMargin() {
    InductorCandidate candidate;
    candidate.passed = true;
    //SaturationValidation: calculatedValue IS the margin percent (actualIsTheMargin=true) -> 11.0,
    //normalized = 11.0/10.0 = 1.1 (large headroom)
    //ThermalValidation: margin = 40-35 = 5, normalized = 5/40 = 0.125 (tight headroom - the real bottleneck)
    candidate.validations = {
        makeValidation("SaturationValidation", EvaluationStatus::Evaluated, true, 11.0, 10.0),
        makeValidation("ThermalValidation", EvaluationStatus::Evaluated, true, 35.0, 40.0),
        makeValidation("InductanceValidation", EvaluationStatus::Evaluated, true, 1.0, 10.0),
    };

    BottleneckAnalysis result = analyzeBottleneck(candidate);

    assert(result.reason == BottleneckReason::MarginHeadroom);
    assert(result.limitingCheckName == "ThermalValidation");
    assert(approxEqual(result.marginValue, 0.125, 1e-6));
    std::printf("testPassingAllEvaluatedPicksSmallestMargin: limitingCheckName=%s (normalizedMargin=%.4f)\n",
                result.limitingCheckName.c_str(), result.marginValue);
}

//7. CurrentConsistencyValidation must never be selected as the bottleneck, even when its raw margin
//would otherwise dominate the comparison (it's diagnostic, not a physical gate).
void testCurrentConsistencyExcludedFromMarginComparison() {
    InductorCandidate candidate;
    candidate.passed = true;
    //CurrentConsistencyValidation: limitValue=0.0 (its real convention), calculatedValue=1000.0 ->
    //raw margin = 0.0 - 1000.0 = -1000.0, which WOULD dominate as "worst" if not excluded.
    candidate.validations = {
        makeValidation("CurrentConsistencyValidation", EvaluationStatus::Evaluated, true, 1000.0, 0.0),
        makeValidation("ThermalValidation", EvaluationStatus::Evaluated, true, 35.0, 40.0),
    };

    BottleneckAnalysis result = analyzeBottleneck(candidate);

    assert(result.reason == BottleneckReason::MarginHeadroom);
    assert(result.limitingCheckName == "ThermalValidation");
    std::printf("testCurrentConsistencyExcludedFromMarginComparison: limitingCheckName=%s (CurrentConsistencyValidation correctly excluded)\n",
                result.limitingCheckName.c_str());
}

}  // namespace

void runBottleneckAnalysisTests() {
    testRejectedWithOneFailure();
    testRejectedWithMultipleFailuresPicksClosestToPassing();
    testEarlyReturnNonConvergenceFallback();
    testPassingWithSaturationNotEvaluated();
    testPriorityPicksSaturationOverBundleFit();
    testPassingAllEvaluatedPicksSmallestMargin();
    testCurrentConsistencyExcludedFromMarginComparison();
    std::printf("All BottleneckAnalysisTests passed.\n");
}
