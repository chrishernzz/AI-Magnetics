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

    rules.maximumRippleCurrentPercent = 100.0;

    rules.recommendedFluxDerateFactor = 0.85;

    rules.minManufacturableGapMm = 0.05;
    rules.gapStepMm = 0.01;
    rules.maxGapFraction = 0.4;
    rules.gapTolerancePercent = 10.0;
    rules.gapMethod = GapMethod::MachinedCenterLeg;

    rules.singleBuildInsulationBuildUpMm = 0.05;
    rules.packingFactor = 0.75;
    rules.bobbinWindowDerateFactor = 0.85;
    rules.marginAllowanceAreaFraction = 0.03;
    rules.leadExitAllowanceAreaFraction = 0.03;
    rules.currentSharingDerateFactor = 0.9;

    rules.totalLeadLengthAllowanceMm = 50.0;
    rules.routingLengthAllowanceMm = 20.0;
    rules.connectionResistanceMilliOhm = 0.5;
    rules.copperTempCoefficientPerC = 0.00393;
    rules.assumedWindingTempCWhenThermalNotEvaluated = 90.0;

    rules.defaultThermalResistanceCPerW = 15.0;
    rules.naturalConvectionCoefficientWPerM2K = 10.0;
    rules.compactSolidSurfaceAreaShapeFactor = 6.0;
    rules.thermalConvergenceThresholdC = 0.1;
    rules.maxThermalIterations = 20;

    rules.skinDepthRiskModerateThreshold = 1.0;
    rules.skinDepthRiskHighThreshold = 2.0;

    return rules;
}
