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

//precondition: candidate.recommendation, candidate.lossSummary, and candidate.manufacturabilityMarginPercent have already been computed
//postcondition: returns a human-readable sentence combining the tier classification with the concrete numbers that drove it
std::string explainRanking(const InductorCandidate& candidate);
