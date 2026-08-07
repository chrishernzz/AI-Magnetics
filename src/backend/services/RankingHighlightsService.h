#pragma once
#include <vector>
#include "core/model/InductorCandidate.h"
#include "core/model/RankingHighlights.h"

RankingHighlights computeRankingHighlights(const std::vector<InductorCandidate>& candidates);
