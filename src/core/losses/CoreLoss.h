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
