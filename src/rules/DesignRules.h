#pragma once

// Explicit engineering-rules layer (spec section 7): every default magnetic
// design constant used by the pipeline lives here, in C++, with units and a
// source noted — never hard-coded inside a FastAPI route.

// STAGE: Design Rules
//
// These are Phase 1 defaults, not measured material facts. Where a
// material-specific limit exists in the data (e.g. BmaxT from
// real_materials.csv), callers should prefer it over defaultFluxDensityLimitT
// and flag that the default was NOT used (see ValidationResult.usedDefaultLimit).

struct DesignRules {
    // Ku - window utilization factor (fraction of window area assumed
    // available for copper after bobbin/insulation/margin losses).
    // Source: McLyman, "Transformer and Inductor Design Handbook", typical
    // value for a simple round-wire, single-winding inductor.
    double windowUtilization;

    // J - allowable current density, A/cm^2. Source: McLyman, conservative
    // natural-convection design guideline for power inductors.
    double allowableCurrentDensityAperCm2;

    // Bmax - default peak flux density limit, T. This is a Phase 1
    // fallback used ONLY when a material does not carry its own measured
    // BmaxT (true for every material in real_materials.csv today - see
    // docs/DATA_FILES.md). Ferrite materials typically saturate well above
    // this; it is a conservative default, not a material-specific fact.
    double defaultFluxDensityLimitT;

    // Minimum required margin (%) between calculated peak flux density and
    // the applicable Bmax limit (default or material-specific) before a
    // candidate is accepted by SaturationValidation.
    double minimumSaturationMarginPercent;

    // Maximum acceptable winding fill factor (copper area / window area)
    // before a candidate is rejected by WindingFitValidation.
    double maximumFillFactor;

    // Default inductance tolerance (%) used when the request does not
    // supply inductanceTolerancePercent.
    double defaultInductanceTolerancePercent;

    // Finest (highest-numbered) single-strand AWG gauge WindingDesign will
    // recommend before switching to parallel strands of a coarser gauge.
    // This is a hand-winding/manufacturability heuristic, not physics -
    // documented separately from the AWG reference table it's compared
    // against (src/data/AwgTable.h).
    int minimumSingleStrandAwg;

    // Named Phase 1 ruleset. Values above are applied here so the numbers
    // and their sourcing comments live in exactly one place.
    static DesignRules phase1Default();
};
