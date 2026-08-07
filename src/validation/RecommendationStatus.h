#pragma once
#include <string>
#include <vector>
#include "validation/Validation.h"
#include "core/losses/SkinDepthRisk.h"

/*

STAGE 10: 3-tier recommendation status

Replaces the frontend's old binary "Recommended"/not sugar (isRecommended: i===0, first-in-list only) with a
real, backend-computed 3-tier classification: PASS / CONDITIONAL_PASS / REJECT. determineRecommendationStatus()
is the sole source of truth for this classification - no other code path may assign a tier. Reject always
mirrors (never contradicts) the existing candidate.passed bool computed from the mandatory validation checks
- this module never overrides that decision, only adds finer-grained honesty on top of a candidate that
already passed every mandatory check that ran.

PASS requires every MANDATORY check to be both Evaluated and passed, with no default-assumption fallback and
no preliminary-estimate check in play. Important, deliberate consequence of this Phase 1 dataset: PASS is
currently UNREACHABLE. ThermalValidation sets isPreliminaryEstimate=true on every result it ever produces
(ThermalEvaluation.h - thermal never exceeds PreliminaryThermalEstimate, by construction, since
defaultThermalResistanceCPerW is always a coarse Phase 1 constant, never per-core data). Since ThermalValidation
is mandatory, no candidate in Phase 1 can ever reach PASS - the tier exists as the schema's top rung for when
real Rth and real AC-loss-watts data eventually exist, not as a claim that any design produced by this engine
today is unconditionally ready to build. This mirrors the same honest "structurally unreachable" pattern as
LossSummary::isCompleteTotal (core/model/LossSummary.h).

*/

enum class RecommendationTier {
    Pass,
    ConditionalPass,
    Reject,
};

struct RecommendationClassification {
    RecommendationTier tier = RecommendationTier::Reject;
    int checksEvaluatedCount = 0;
    int checksPassedCount = 0;
    int checksFailedCount = 0;
    int checksNotEvaluatedCount = 0;
    std::vector<std::string> missingInfo;
    std::string explanation;
};

RecommendationClassification determineRecommendationStatus(bool passed, const std::vector<ValidationResult>& validations, const SkinDepthRiskResult& acLossRisk);
