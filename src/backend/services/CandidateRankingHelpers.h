#pragma once
#include "core/model/InductorCandidate.h"

/*

Shared per-check margin/ranking primitives, extracted from InductorDesignService.cpp's
candidateRanksAhead() comparator (unchanged) so BottleneckAnalysisService and
RankingHighlightsService can reuse the exact same "how good is this check's result" logic instead
of re-deriving it.

*/

const ValidationResult* findValidation(const InductorCandidate& candidate, const char* checkName);
double thermalRiseForRanking(const InductorCandidate& candidate);
double marginForRanking(const InductorCandidate& candidate, const char* checkName, bool actualIsTheMargin);
