#pragma once
#include <vector>
#include "core/model/InductorCandidate.h"
#include "core/model/RankingHighlights.h"

//precondition: candidates is the already-sorted, already-passing candidates list (recommendation.candidates)
//postcondition: identifies the best-in-category candidate by partNumber for each of the four criteria -
//see RankingHighlights.h. Does not read or modify the sort order. hasCandidates is false (and every
//other field is left default/empty) when candidates is empty - never a fabricated part number.
RankingHighlights computeRankingHighlights(const std::vector<InductorCandidate>& candidates);
