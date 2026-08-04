#pragma once
#include <string>
#include "core/model/InductorCandidate.h"

/*

STAGE 12: Per-candidate ranking explanation (spec section 11)

Extends, rather than duplicates, the single existing aggregate DesignRecommendation.message ("N candidate(s)
passed every check; M rejected") with a real sentence explaining where THIS candidate landed and why -
called once per candidate after recommendation/lossSummary/manufacturabilityMarginPercent have all been
populated (see InductorDesignService.cpp).

*/

std::string explainRanking(const InductorCandidate& candidate);

//precondition: candidate.bottleneck has already been computed (see BottleneckAnalysisService.h)
//postcondition: rule-based engineering suggestion tied directly to candidate.bottleneck - no search, no
//new solver run, never a fabricated specific numeric alternative. When the bottleneck is missing peak
//current specifically, always returns the exact phrase "supply peak current to evaluate saturation
//risk" rather than a generic guess. Never returns an empty string.
std::string suggestImprovement(const InductorCandidate& candidate);
