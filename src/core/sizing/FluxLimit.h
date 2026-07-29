#pragma once
#include "core/sizing/MaterialEvaluation.h"
#include "rules/DesignRules.h"

// Single source of truth for "which peak-flux-density limit applies to this material" - previously
// duplicated logic living only in an anonymous namespace inside DesignValidation.cpp (used by
// PeakFluxValidation/SaturationValidation/calculateFluxLimitTiers), now also needed by
// TurnsAndGapDesign.cpp's flux-aware turns seed (see designTurnsAndGap()) - moved here so both
// translation units share one implementation instead of two copies that could drift out of sync.
struct FluxLimit {
    double limitT;
    bool usedDefault;
};

// precondition: none
// postcondition: material.hasBmaxData ? {material.bmaxT, false} : {rules.defaultFluxDensityLimitT, true}
FluxLimit applicableFluxLimit(const MaterialCandidate& material, const DesignRules& rules);
