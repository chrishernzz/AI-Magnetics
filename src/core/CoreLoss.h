#pragma once

// STAGE 8b: Core loss
//
// Phase 1 simplified loss-density model, only meaningful once a material
// carries a validated core-loss coefficient (real_materials.csv's
// CuLossFactor column exists but is 0.0/unpopulated for every material
// today - see docs/DATA_FILES.md). LossEvaluation must check
// MaterialCandidate::hasCoreLossData before calling this - it must never be
// called with a placeholder coefficient and presented as a real result.
//
// Model: Pv (W/cm^3) = cuLossFactor * (switchingFreqHz / 100000.0) *
// (fluxDensitySwingT / 0.1)^2 - a generic frequency-linear,
// flux-swing-squared simplification, NOT a fitted Steinmetz curve. Replace
// with real per-material Steinmetz coefficients (k, alpha, beta) when that
// data becomes available.
double calculateCoreLoss(double cuLossFactor, double fluxDensitySwingT, double switchingFreqHz);
