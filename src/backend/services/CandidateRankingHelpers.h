#pragma once
#include "core/model/InductorCandidate.h"

/*

Shared per-check margin/ranking primitives, extracted from InductorDesignService.cpp's
candidateRanksAhead() comparator (unchanged) so BottleneckAnalysisService and
RankingHighlightsService can reuse the exact same "how good is this check's result" logic instead
of re-deriving it.

*/

//precondition: candidate.validations was built with InductanceValidation/PeakFluxValidation/SaturationValidation/
//WindingFitValidation/CurrentDensityValidation/BundleFitValidation/ThermalValidation, in that order (see InductorDesignService.cpp's evaluateCandidate())
//postcondition: returns a pointer to the named check, or nullptr if not found (defensive - never crashes on a mismatch)
const ValidationResult* findValidation(const InductorCandidate& candidate, const char* checkName);

//precondition: none
//postcondition: lower-is-better ranking value for predicted temperature rise; a not_evaluated thermal result
//ranks as the worst possible (never as if it were a benign 0C rise).
double thermalRiseForRanking(const InductorCandidate& candidate);

//precondition: none
//postcondition: higher-is-better ranking value (limit - actual) for the named check; a not_evaluated check
//ranks as the worst possible (never as if it had unlimited margin).
double marginForRanking(const InductorCandidate& candidate, const char* checkName, bool actualIsTheMargin);
