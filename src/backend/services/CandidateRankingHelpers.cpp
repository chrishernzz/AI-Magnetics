#include "backend/services/CandidateRankingHelpers.h"
#include <limits>

//precondition: see header
//postcondition: see header
const ValidationResult* findValidation(const InductorCandidate& candidate, const char* checkName) {
    for (const auto& v : candidate.validations) {
        if (v.checkName == checkName) {
            return &v;
        }
    }
    return nullptr;
}

//precondition: see header
//postcondition: see header
double thermalRiseForRanking(const InductorCandidate& candidate) {
    const ValidationResult* v = findValidation(candidate, "ThermalValidation");
    if (v == nullptr || v->status != EvaluationStatus::Evaluated) {
        return std::numeric_limits<double>::max();
    }
    return v->calculatedValue;
}

//precondition: see header
//postcondition: see header
double marginForRanking(const InductorCandidate& candidate, const char* checkName, bool actualIsTheMargin) {
    const ValidationResult* v = findValidation(candidate, checkName);
    if (v == nullptr || v->status != EvaluationStatus::Evaluated) {
        return -std::numeric_limits<double>::max();
    }
    return actualIsTheMargin ? v->calculatedValue : (v->limitValue - v->calculatedValue);
}
