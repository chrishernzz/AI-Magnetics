#pragma once
#include <string>
#include <vector>
#include "validation/Validation.h"
#include "core/losses/SkinDepthRisk.h"

/*

STAGE 10: 3-tier recommendation status (spec section 10)

Replaces the frontend's old binary "Recommended"/not sugar (isRecommended: i===0, first-in-list only) with a
real, backend-computed 3-tier classification. Rejected always mirrors (never contradicts) the existing
candidate.passed bool computed from the mandatory validation checks - this module never overrides that
decision, only adds finer-grained honesty on top of a candidate that already passed.

Important, deliberate consequence of this Phase 1 dataset: Phase1Recommended is currently UNREACHABLE.
ThermalValidation sets isPreliminaryEstimate=true on every result it ever produces (ThermalEvaluation.h -
thermal never exceeds PreliminaryThermalEstimate, by construction, since defaultThermalResistanceCPerW is
always a coarse Phase 1 constant, never per-core data). Since Phase1Recommended requires zero preliminary
checks among everything mandatory, and thermal is mandatory, no candidate in Phase 1 can ever reach it - the
tier exists as the schema's top rung for when real Rth and real AC-loss-watts data eventually exist, not as
a claim that any design produced by this engine today is unconditionally ready to build. This mirrors the
same honest "structurally unreachable" pattern as LossSummary::isCompleteTotal (core/model/LossSummary.h).

*/

enum class RecommendationTier {
    Phase1Recommended,
    PreliminaryCandidate,
    Rejected,
};

struct RecommendationClassification {
    RecommendationTier tier = RecommendationTier::Rejected;
    int checksEvaluatedCount = 0;
    int checksPassedCount = 0;
    int checksFailedCount = 0;
    int checksNotEvaluatedCount = 0;
    std::vector<std::string> missingInfo;
    std::string explanation;
};

//precondition: validations is the candidate's full validation list (same one candidate.passed was derived from)
//postcondition: tier == Rejected whenever passed == false (mirrors, never contradicts, the existing pass/fail
//decision). Otherwise PreliminaryCandidate when any check is NotEvaluated, any check carries
//isPreliminaryEstimate, or acLossRisk.riskLevel is Moderate/High (acLossRisk.acLossWattsStatus is always
//NotEvaluated in Phase 1 too - see SkinDepthRisk.h). Phase1Recommended only when none of those apply - see
//the block comment above for why that is currently unreachable given this engine's real data coverage.
RecommendationClassification classifyRecommendation(bool passed, const std::vector<ValidationResult>& validations,
                                                      const SkinDepthRiskResult& acLossRisk);
