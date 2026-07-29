#include "backend/services/RankingHighlightsService.h"
#include "backend/services/CandidateRankingHelpers.h"

//precondition: see header
//postcondition: see header
RankingHighlights computeRankingHighlights(const std::vector<InductorCandidate>& candidates) {
    RankingHighlights result;
    if (candidates.empty()) {
        return result;
    }
    result.hasCandidates = true;

    double bestThermalRise = thermalRiseForRanking(candidates.front());
    double lowestLoss = candidates.front().lossSummary.knownPartialLossW;
    //marginForRanking returns -max() for any candidate whose SaturationValidation is NotEvaluated - if
    //every candidate in this list is in that state, the "highest" margin found is just the first
    //candidate by iteration order (an arbitrary but harmless tie among equally-unknown values, never
    //treated as a real measured margin).
    double highestSatMargin = marginForRanking(candidates.front(), "SaturationValidation", true);
    double smallestAreaProduct = candidates.front().core.areaProductCm4;

    result.thermalBestPartNumber = candidates.front().core.partNumber;
    result.lowestLossPartNumber = candidates.front().core.partNumber;
    result.highestSaturationMarginPartNumber = candidates.front().core.partNumber;
    result.smallestCorePartNumber = candidates.front().core.partNumber;

    for (std::size_t i = 1; i < candidates.size(); ++i) {
        const InductorCandidate& c = candidates[i];

        double thermalRise = thermalRiseForRanking(c);
        if (thermalRise < bestThermalRise) {
            bestThermalRise = thermalRise;
            result.thermalBestPartNumber = c.core.partNumber;
        }

        if (c.lossSummary.knownPartialLossW < lowestLoss) {
            lowestLoss = c.lossSummary.knownPartialLossW;
            result.lowestLossPartNumber = c.core.partNumber;
        }

        double satMargin = marginForRanking(c, "SaturationValidation", true);
        if (satMargin > highestSatMargin) {
            highestSatMargin = satMargin;
            result.highestSaturationMarginPartNumber = c.core.partNumber;
        }

        if (c.core.areaProductCm4 < smallestAreaProduct) {
            smallestAreaProduct = c.core.areaProductCm4;
            result.smallestCorePartNumber = c.core.partNumber;
        }
    }

    return result;
}
