#pragma once
#include "core/magnetics/GapMethod.h"

//Explicit engineering-rules layer (spec section 7): every default magnetic
//design constant used by the pipeline lives here, in C++, with units and a
//source noted — never hard-coded inside a FastAPI route.

//STAGE: Design Rules
//
//These are Phase 1 defaults, not measured material facts. Where a
//material-specific limit exists in the data (e.g. BmaxT from
//real_materials.csv), callers should prefer it over defaultFluxDensityLimitT
//and flag that the default was NOT used (see ValidationResult.usesDefaultAssumption).
//
//Every field added after minimumSingleStrandAwg is a "Phase 1 completion"
//addition - each is a documented engineering estimate or a real physical
//constant, never a fabricated per-part fact. See docs/DATA_FILES.md and
//the project's honesty-ledger notes for which fields are estimates vs.
//real constants.

struct DesignRules {
    //Ku - window utilization factor (fraction of window area assumed
    //available for copper after bobbin/insulation/margin losses).
    //Source: McLyman, "Transformer and Inductor Design Handbook", typical
    //value for a simple round-wire, single-winding inductor.
    double windowUtilization;

    //J - allowable current density, A/cm^2. Was McLyman's 400 A/cm^2
    //"conservative natural-convection" figure; a real user report (Roger,
    //senior magnetics engineer) flagged that figure as nearly 2.5x
    //stricter than the wire-selection outcome he expects, and hand-checked
    //against the standard "200 circular mils per amp" rule of thumb for a
    //~40C rise design instead. 200 CM/A converts to A/cm^2 as:
    //  1 circular mil = (pi/4) * (0.001 in)^2 = 5.06707e-6 cm^2
    //  200 CM/A = 200 * 5.06707e-6 cm^2/A = 1.013414e-3 cm^2/A per amp
    //  J = 1 / (area per amp) = 986.76 A/cm^2
    //Both 400 and ~987 A/cm^2 are real, citable rules of thumb from
    //different sources (McLyman's handbook table vs. the classic
    //circular-mils-per-amp heuristic) - they simply don't agree, and which
    //one is "right" depends on winding geometry/airflow/insulation class
    //this Phase 1 model doesn't yet capture per-design. Using Roger's
    //number here since it's the one this tool is being validated against.
    double allowableCurrentDensityAperCm2;

    //Bmax - default peak flux density limit, T. This is a Phase 1
    //fallback used ONLY when a material does not carry its own measured
    //BmaxT (true for every material in real_materials.csv today - see
    //docs/DATA_FILES.md). Ferrite materials typically saturate well above
    //this; it is a conservative default, not a material-specific fact.
    double defaultFluxDensityLimitT;

    //Minimum required margin (%) between calculated peak flux density and
    //the applicable Bmax limit (default or material-specific) before a
    //candidate is accepted by SaturationValidation.
    double minimumSaturationMarginPercent;

    //Maximum acceptable winding fill factor (copper area / window area)
    //before a candidate is rejected by WindingFitValidation.
    double maximumFillFactor;

    //Default inductance tolerance (%) used when the request does not
    //supply inductanceTolerancePercent.
    double defaultInductanceTolerancePercent;

    //Finest (highest-numbered) single-strand AWG gauge WindingDesign will
    //recommend before switching to parallel strands of a coarser gauge.
    //This is a hand-winding/manufacturability heuristic, not physics -
    //documented separately from the AWG reference table it's compared
    //against (src/data/AwgTable.h).
    int minimumSingleStrandAwg;

    //Safety margin (%) applied to the raw current-density-derived required
    //copper area before a wire gauge is picked - e.g. 30% means the chosen
    //wire must clear 1.3x the theoretical minimum area, not just barely
    //clear the raw minimum with near-zero headroom. A real user report
    //(Roger, senior magnetics engineer) flagged that picking the exact
    //thinnest wire that mathematically clears allowableCurrentDensityAperCm2
    //(e.g. AWG20 clearing a 5A/986.76A/cm^2 requirement by only ~2%) is bad
    //practice - real DCR varies with temperature/manufacturing tolerance,
    //so a design should never sit right at the edge of its current-density
    //limit. This 30% figure is a generic conservative engineering margin,
    //NOT sourced from Roger or a specific published wire-ampacity standard -
    //an honest judgment call, not a fact, and should be revisited if he
    //gives a real number to use instead.
    double wireAreaSafetyMarginPercent;

    //--- Buck-converter input validation ---

    //Upper input-sanity bound on requested ripple current, as a percent
    //of Iout. Above ~100% the triangular-ripple CCM model this engine
    //assumes becomes unreliable and the operating point is likely already
    //in DCM (see BuckElectricalSolver's own CCM/DCM check) - this is a
    //Phase 1 input-sanity default, not a manufacturer limit.
    double maximumRippleCurrentPercent;

    //--- Flux limit tiers (informational, see calculateFluxLimitTiers) ---

    //Generic engineering derate applied to the absolute saturation flux
    //limit to produce a "recommended operating" flux limit. Not
    //material-specific data - a Phase 1 default margin.
    double recommendedFluxDerateFactor;

    //--- Gap tolerance / manufacturability ---

    //Below this gap length, a machined/lapped center-leg gap is not
    //reliably reproducible by hand - a Phase 1 estimate, not a
    //machine-shop spec. Triggers a warning, not a rejection.
    double minManufacturableGapMm;

    //Machining resolution increment the calculated gap is rounded to.
    //Moved here from a local constant in TurnsAndGapDesign.cpp so it is
    //configurable and documented in one place.
    double gapStepMm;

    //Practical upper bound on gap length, as a fraction of the core's
    //magnetic path length. Moved here from a local constant in
    //TurnsAndGapDesign.cpp for the same reason as gapStepMm.
    double maxGapFraction;

    //Typical mechanical tolerance on a machined/lapped gap, expressed as
    //a percent of the nominal gap length - a Phase 1 generic estimate
    //used to sweep inductance at the gap's min/max extremes.
    double gapTolerancePercent;

    //Which gapping technique the pipeline assumes. Only MachinedCenterLeg
    //has a validated formula in Phase 1 - see GapMethod.h.
    GapMethod gapMethod;

    //--- Winding realism ---

    //Typical single-build magnet-wire enamel thickness added to a bare
    //conductor's diameter to get its insulated diameter. A generic
    //order-of-magnitude estimate, not tied to a specific NEMA MW1000
    //insulation class.
    double singleBuildInsulationBuildUpMm;

    //Achievable winding packing density beyond a simple area-sum of
    //insulated conductors - a McLyman-style estimate.
    double packingFactor;

    //Fraction of a core's raw window area (Wa) usable after bobbin wall
    //thickness is accounted for. No per-core bobbin geometry exists in
    //the data, so this is a Phase 1 generic estimate. Applied only to
    //non-toroid cores (see WindingDesign.cpp) - a toroid has no separate
    //bobbin former at all, so charging it this derate overstated how much
    //window space a toroid actually loses. A real user report (a senior
    //magnetics engineer's hand calculation) caught this - it was the
    //single largest identifiable cause of powder-core toroids (the
    //majority of high-current powder candidates, which already need far
    //more turns than ferrite for the same inductance) failing
    //WindingFitValidation on a derate that doesn't apply to their real
    //construction.
    double bobbinWindowDerateFactor;

    //Creepage/clearance margin, expressed as a fraction of the raw window
    //area (the core data has no width/height split, only a raw area - see
    //docs/DATA_FILES.md for why this is an area fraction rather than a
    //literal millimeter dimension).
    double marginAllowanceAreaFraction;

    //Window area reserved for lead dress/exit, same area-fraction
    //reasoning as marginAllowanceAreaFraction.
    double leadExitAllowanceAreaFraction;

    //Derates a parallel-strand bundle's effective ampacity for imperfect
    //current sharing between strands - a Phase 1 generic estimate.
    double currentSharingDerateFactor;

    //--- DCR / copper loss realism ---

    //Combined estimated lead length (both terminations), winding exit to
    //termination point - a Phase 1 generic estimate, not per-core
    //measured data.
    double totalLeadLengthAllowanceMm;

    //Additional estimated routing length beyond direct leads (PCB trace,
    //busbar run, etc.) - a Phase 1 generic estimate.
    double routingLengthAllowanceMm;

    //Estimated termination/connection resistance (crimp, solder joint,
    //pad) - a Phase 1 generic estimate, not measured.
    double connectionResistanceMilliOhm;

    //Copper resistivity temperature coefficient, 1/degC at 20degC
    //reference. This is a real IACS physical constant (not a Phase 1
    //guess) - centralized here alongside the other constants it is used
    //with, so DCR temperature correction has exactly one source for it.
    double copperTempCoefficientPerC;

    //Conservative fallback winding temperature (degC) used to estimate a
    //"hot" DCR ONLY when the real thermal iterative loop cannot run
    //(e.g. DCR geometry unknown) - not a converged answer, just a
    //sanity-check upper bound.
    double assumedWindingTempCWhenThermalNotEvaluated;

    //--- Thermal (Phase 1 default Rth) ---

    //Fallback thermal-resistance (degC/W) used ONLY when a candidate's core
    //geometry (Ae/Le) is unavailable to derive the size-aware estimate below -
    //never per-core measured or simulated data either way, so ThermalEvaluation
    //can only ever report a PreliminaryThermalEstimate, never a full pass,
    //regardless of how complete upstream loss coverage is. Typical real-world
    //Rth for inductors this size ranges roughly 5-40 degC/W depending on actual
    //size/surface area/airflow - this flat number used to be applied to every
    //core regardless of size; see naturalConvectionCoefficientWPerM2K below for
    //the real per-core replacement.
    double defaultThermalResistanceCPerW;

    //Natural-convection heat-transfer coefficient, W/(m^2*K), used to turn a
    //candidate's own real core geometry into a size-aware thermal resistance
    //via Newton's law of cooling (Rth = 1 / (h * surfaceAreaM2)) - see
    //ThermalEvaluation.cpp. Real, citable natural-convection-in-still-air
    //range for small electronics-scale components is ~5-25 W/(m^2*K); 10 was
    //the prior default (the commonly cited "typical" value). Raised to 25 -
    //the top of that same cited range, NOT independently sourced - as a
    //working test value while cross-checking against a real user report
    //(Roger, senior magnetics engineer, C055439A2: hand-calculated 20-30C
    //rise vs. this tool's much higher prediction). Combined with the real
    //wound-coil surface area fix (coreWoundSurfaceAreaMm2), this value lands
    //C055439A2's predicted rise at ~22.6C, inside his stated range - but this
    //number is still unconfirmed by Roger himself and should be replaced
    //with whatever real h (or equivalent Rth) he actually uses once he gives
    //one, the same way allowableCurrentDensityAperCm2 was replaced with his
    //stated 200 CM/A rule rather than left as a guess.
    double naturalConvectionCoefficientWPerM2K;

    //Shape factor relating a core's magnetic volume (Ae*Le) to an estimated
    //total external surface area, assuming a roughly compact/cube-like solid
    //(surfaceArea ~= factor * volume^(2/3) - the real cube surface-to-volume
    //relation at factor=6). This does not yet differentiate real shape
    //families: a toroid's actual surface-to-volume ratio is higher than a
    //cube's (so this under-estimates a toroid's real surface area, i.e. is
    //conservative - it overstates rather than overclaims a toroid's real
    //cooling); a squat two-piece ferrite core is a closer fit. A documented
    //Phase 1 order-of-magnitude simplification, not a fabricated per-shape fact.
    double compactSolidSurfaceAreaShapeFactor;

    //Winding-temperature change (degC) below which the thermal iterative
    //loop is considered converged.
    double thermalConvergenceThresholdC;

    //Iteration cap for the thermal loop, mirroring
    //TurnsAndGapDesign.cpp's existing turns/gap convergence-loop pattern
    //(kMaxIterations).
    int maxThermalIterations;

    //--- Skin-depth AC-loss risk heuristic ---

    //strandRadius/skinDepth ratio at or below which AC-loss risk is
    //classified Low. A Phase 1 heuristic threshold, not a validated
    //AC-loss boundary from a Dowell/FEA model.
    double skinDepthRiskModerateThreshold;

    //strandRadius/skinDepth ratio above which AC-loss risk is classified
    //High. Same heuristic-not-validated caveat as
    //skinDepthRiskModerateThreshold.
    double skinDepthRiskHighThreshold;

    //Named Phase 1 ruleset. Values above are applied here so the numbers
    //and their sourcing comments live in exactly one place.
    static DesignRules phase1Default();
};
