#pragma once
#include <string>

/*

STAGE 15: Ranking highlights (best-in-category)

Non-disruptive complement to the primary candidate ranking (InductorDesignService.cpp's
candidateRanksAhead()) - the primary sort/tier order is unchanged. This identifies, among the
already-ranked passing candidates, which single one is best on each of a few individual criteria an
engineer might care about even when it isn't the overall top-ranked pick.

Identified by core partNumber, not list index - an index is fragile against any future re-sort,
filter, or pagination of the candidates list downstream (a very plausible UI feature), and would
silently point at the wrong candidate with no detectable inconsistency. partNumber is already the
deterministic final tiebreak in candidateRanksAhead() and is self-describing in the API response.
Documented assumption (not structurally enforced): no two candidates in one response share a
partNumber - true today given how findSuitableCores() iterates the core database, but not proven.

*/
struct RankingHighlights {
    //false when recommendation.candidates is empty (no_feasible_design) - every *PartNumber field below
    //is meaningless when this is false, never a fabricated/default part number.
    bool hasCandidates = false;
    std::string thermalBestPartNumber;
    std::string lowestLossPartNumber;
    std::string highestSaturationMarginPartNumber;
    std::string smallestCorePartNumber;
};
