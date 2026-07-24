#pragma once
#include <string>
#include "data/CoreLossCoefficientDatabase.h"

/*

STAGE 8b: Core loss
Phase 1 simplified loss-density model, only meaningful once a material
carries a validated core-loss coefficient (real_materials.csv's
CuLossFactor column exists but is 0.0/unpopulated for every material
today - see docs/DATA_FILES.md). LossEvaluation must check
MaterialCandidate::hasCoreLossData before calling this - it must never be
called with a placeholder coefficient and presented as a real result.

Model: Pv (W/cm^3) = cuLossFactor * (switchingFreqHz / 100000.0) *
(fluxDensitySwingT / 0.1)^2 - a generic frequency-linear,
flux-swing-squared simplification, NOT a fitted Steinmetz curve. Replace
with real per-material Steinmetz coefficients (k, alpha, beta) when that data becomes available.

*/

double calculateCoreLoss(double cuLossFactor, double fluxDensitySwingT, double switchingFreqHz);

/*

Real per-material Steinmetz coefficient lookup, sourced from
data/real_core_loss_coefficients.csv (loaded into CoreLossCoefficientDatabase
at startup - see python/app.py). Deliberately kept separate from the
loss-density formula above: this function only finds the right coefficient
row, it does not compute a loss. That split lets a future
calculateCoreLossDensity(coefficients, fluxDensitySwingT, switchingFreqHz,
temperatureC) reuse this same lookup without duplicating the search logic -
and lets a future permeability/mu-optimization step reuse it too.

Not yet called from LossEvaluation.cpp - core loss is still reported
not_evaluated in Phase 1 pending a decision on where the flux-density-swing
input comes from (see docs/FORMULAS.md).

*/
struct CoreLossCoefficientLookup {
    bool found = false;
    //valid only when found == true
    CoreLossCoefficientData coefficients;
};

//precondition: none
//postcondition: returns the CoreLossCoefficientDatabase row for materialName whose [minFreqHz, maxFreqHz) range contains switchingFreqHz, if any. found == false if the material has no row at all, or has rows but none covering this frequency.
CoreLossCoefficientLookup findCoreLossCoefficients(const std::string& materialName, double switchingFreqHz);

/*

Real Steinmetz core-loss density, using a coefficient row returned by
findCoreLossCoefficients() above - the two are deliberately separate
functions (lookup vs. calculation), same reasoning as findCoreLossCoefficients's
own comment. Only meaningful when coefficients came from a real, matching
row (LossEvaluation.cpp only calls this when CoreLossCoefficientLookup::found
is true).

Model: Pv (W/m^3) = k * f^alpha * B^beta - the standard Steinmetz equation,
f in Hz, B (fluxDensitySwingT) in tesla. W/m^3, not W/cm^3 - these
coefficients come from PyOpenMagnetics/MAS's "volumetricLosses" field
(SI convention), confirmed empirically against real values (treating
the result as W/cm^3 produces physically absurd loss densities). This
does NOT apply the coefficient row's temperature-correction terms
(ct0/ct1/ct2) yet - their exact formula isn't confirmed against the
upstream source, and applying a guessed correction would be worse than
not applying one. Known Phase 1 gap, not a silent omission - see
docs/FORMULAS.md section 9.

*/
double calculateCoreLossDensity(const CoreLossCoefficientData& coefficients, double fluxDensitySwingT, double switchingFreqHz);

/*

Flux-swing valid-range guard, same "never extrapolate silently" rule already applied to frequency in
findCoreLossCoefficients() above. Checked separately from the lookup because the swing value isn't known
until turns/gap and ripple current have both been resolved (see LossEvaluation.cpp).

*/

//precondition: none
//postcondition: returns true if fluxDensitySwingT falls within [minFluxSwingT, maxFluxSwingT], OR if either
//bound is absent (nothing to check against - see CoreLossCoefficientDatabase.h, currently always the case).
//Returns false only when a present bound is actually violated.
bool fluxSwingWithinValidatedRange(const CoreLossCoefficientData& coefficients, double fluxDensitySwingT);
