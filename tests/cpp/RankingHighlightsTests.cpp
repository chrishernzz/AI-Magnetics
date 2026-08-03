#include <cassert>
#include <cstdio>
#include <vector>
#include "backend/services/RankingHighlightsService.h"

namespace {

InductorCandidate makeCandidate(const std::string& partNumber, double thermalRiseC, double lossW,
                                 double satMarginPercent, double areaProductCm4) {
    InductorCandidate candidate;
    candidate.core.partNumber = partNumber;
    candidate.core.areaProductCm4 = areaProductCm4;
    candidate.lossSummary.knownPartialLossW = lossW;

    ValidationResult thermal;
    thermal.checkName = "ThermalValidation";
    thermal.status = EvaluationStatus::Evaluated;
    thermal.passed = true;
    thermal.calculatedValue = thermalRiseC;

    ValidationResult saturation;
    saturation.checkName = "SaturationValidation";
    saturation.status = EvaluationStatus::Evaluated;
    saturation.passed = true;
    saturation.calculatedValue = satMarginPercent;

    candidate.validations = {thermal, saturation};
    return candidate;
}

//1. Four synthetic candidates with distinct values on each of the four criteria - each *PartNumber
//field must independently pick the correct winner, not just reuse the same "best overall" candidate.
void testHighlightsPickCorrectCandidatePerCriterion() {
    std::vector<InductorCandidate> candidates = {
        makeCandidate("CoreA", 50.0, 2.0, 5.0, 10.0),
        makeCandidate("CoreB", 20.0, 3.0, 8.0, 8.0),   //best thermal
        makeCandidate("CoreC", 30.0, 1.0, 6.0, 12.0),  //lowest loss
        makeCandidate("CoreD", 40.0, 2.5, 15.0, 3.0),  //highest sat margin AND smallest core
    };

    RankingHighlights result = computeRankingHighlights(candidates);

    assert(result.hasCandidates);
    assert(result.thermalBestPartNumber == "CoreB");
    assert(result.lowestLossPartNumber == "CoreC");
    assert(result.highestSaturationMarginPartNumber == "CoreD");
    assert(result.smallestCorePartNumber == "CoreD");
    std::printf("testHighlightsPickCorrectCandidatePerCriterion: thermalBest=%s lowestLoss=%s highestSatMargin=%s smallestCore=%s\n",
                result.thermalBestPartNumber.c_str(), result.lowestLossPartNumber.c_str(),
                result.highestSaturationMarginPartNumber.c_str(), result.smallestCorePartNumber.c_str());
}

//2. Empty candidates list - hasCandidates must be false, never a fabricated part number.
void testEmptyCandidatesListIsHandledDefensively() {
    std::vector<InductorCandidate> candidates;

    RankingHighlights result = computeRankingHighlights(candidates);

    assert(!result.hasCandidates);
    assert(result.thermalBestPartNumber.empty());
    std::printf("testEmptyCandidatesListIsHandledDefensively: hasCandidates=false, no fabricated part number\n");
}

//3. Single candidate - trivially wins every category.
void testSingleCandidateWinsEveryCategory() {
    std::vector<InductorCandidate> candidates = {makeCandidate("OnlyCore", 30.0, 1.5, 12.0, 5.0)};

    RankingHighlights result = computeRankingHighlights(candidates);

    assert(result.hasCandidates);
    assert(result.thermalBestPartNumber == "OnlyCore");
    assert(result.lowestLossPartNumber == "OnlyCore");
    assert(result.highestSaturationMarginPartNumber == "OnlyCore");
    assert(result.smallestCorePartNumber == "OnlyCore");
    std::printf("testSingleCandidateWinsEveryCategory: all four fields correctly point at the only candidate\n");
}

}  //namespace

void runRankingHighlightsTests() {
    testHighlightsPickCorrectCandidatePerCriterion();
    testEmptyCandidatesListIsHandledDefensively();
    testSingleCandidateWinsEveryCategory();
    std::printf("All RankingHighlightsTests passed.\n");
}
