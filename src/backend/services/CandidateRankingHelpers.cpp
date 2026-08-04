#include "backend/services/CandidateRankingHelpers.h"
#include <limits>

//precondition: candidate.validations was built with InductanceValidation/PeakFluxValidation/SaturationValidation/
//WindingFitValidation/CurrentDensityValidation/BundleFitValidation/ThermalValidation, in that order (see InductorDesignService.cpp's evaluateCandidate())
//postcondition: returns a pointer to the named check, or nullptr if not found (defensive - never crashes on a mismatch)
const ValidationResult* findValidation(const InductorCandidate& candidate, const char* checkName) {
    for (const auto& v : candidate.validations) {
        if (v.checkName == checkName) {
            return &v;
        }
    }
    return nullptr;
}

//precondition: none
//postcondition: lower-is-better ranking value for predicted temperature rise; a not_evaluated thermal result
//ranks as the worst possible (never as if it were a benign 0C rise).
double thermalRiseForRanking(const InductorCandidate& candidate) {
    const ValidationResult* v = findValidation(candidate, "ThermalValidation");
    if (v == nullptr || v->status != EvaluationStatus::Evaluated) {
        return std::numeric_limits<double>::max();
    }
    return v->calculatedValue;
}

//precondition: none
//postcondition: higher-is-better ranking value (limit - actual) for the named check; a not_evaluated check
//ranks as the worst possible (never as if it had unlimited margin).
double marginForRanking(const InductorCandidate& candidate, const char* checkName, bool actualIsTheMargin) {
    const ValidationResult* v = findValidation(candidate, checkName);
    if (v == nullptr || v->status != EvaluationStatus::Evaluated) {
        return -std::numeric_limits<double>::max();
    }
    return actualIsTheMargin ? v->calculatedValue : (v->limitValue - v->calculatedValue);
}
