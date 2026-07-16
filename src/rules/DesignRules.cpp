#include "DesignRules.h"

// precondition: none
// postcondition: returns the single named Phase 1 ruleset. These were
// previously hard-coded inside python/routes/core_selection.py
// (Ku=0.4, Bmax=0.30, J=400) - moved here per spec section 7 so the API
// route layer contains no hidden magnetic-design constants.
DesignRules DesignRules::phase1Default() {
    DesignRules rules;
    rules.windowUtilization = 0.4;
    rules.allowableCurrentDensityAperCm2 = 400.0;
    rules.defaultFluxDensityLimitT = 0.30;
    rules.minimumSaturationMarginPercent = 10.0;
    rules.maximumFillFactor = 0.6;
    rules.defaultInductanceTolerancePercent = 10.0;
    rules.minimumSingleStrandAwg = 18;
    return rules;
}
