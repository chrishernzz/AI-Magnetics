# Requirements traceability

Maps each input/output specified by your boss to the file that owns it, and whether it's actually implemented yet.

| Requirement (from boss) | Owned by | Status |
|---|---|---|
| L (given) | `src/core/sizing/AreaProduct.cpp` · `src/core/magnetics/TurnsCalculation.cpp`/`TurnsAndGapDesign.cpp` | ✅ used in Area Product and in the Phase 1 turns/gap convergence loop |
| Peak current | `src/core/sizing/AreaProduct.cpp` · `DesignValidation.cpp` (PeakFluxValidation/SaturationValidation) | ✅ used in Area Product and peak-flux/saturation checks - never used for RMS-dependent checks |
| RMS current | `RequirementDerivationService.cpp` (direct, or derived from `averageCurrentA`+`rippleCurrentPeakToPeakA` for triangular ripple) · `src/core/winding/WindingDesign.cpp` · `src/core/losses/CopperLoss.cpp` | ✅ required/derivable input, used for winding sizing and DC copper loss; never inferred from peak current |
| Current waveform | Not tracked as its own field - superseded by `InductorDesignRequest.rippleCurrentPeakToPeakA` (see RMS current row) | ⚠️ Only triangular ripple is modeled; the old `waveformFactor` field (on the now-removed `MaterialSelectionInput`) was accepted but never read by anything, so nothing is lost |
| Switching frequency | `src/core/sizing/MaterialEvaluation.cpp` · `src/core/sizing/AreaProduct.cpp` · `src/core/losses/CoreLoss.cpp` (via `findCoreLossCoefficients()`) | ✅ used in material candidate evaluation, Area Product, and to look up the matching real Steinmetz coefficient row for Core Loss |
| Ambient temperature | `InductorDesignRequest.ambientTemperatureC` · `src/core/thermal/ThermalEvaluation.cpp` | ⚠️ Accepted and threaded through; `ThermalEvaluation` always reports `not_evaluated` - no thermal model/data yet |
| Allowable temp rise | `InductorDesignRequest.allowableTempRiseC` · `ThermalValidation` (`DesignValidation.cpp`) | ⚠️ Real check exists and is called; always reports `not_evaluated` (never an assumed pass) pending thermal model/data |
| Ap / core selection (McLyman p.63) | `src/core/sizing/AreaProduct.cpp`, `src/core/sizing/CoreEvaluation.cpp`, `data/real_cores.csv` | ✅ Implemented - Phase 1 path never falls back to an oversized core; returns `no_feasible_design` instead |
| Gapped core branch | `src/core/magnetics/GapDesign.cpp`, `src/core/magnetics/TurnsAndGapDesign.cpp` | ✅ Implemented - series-reluctance model, iterated with turns until convergence |
| Turns | `src/core/magnetics/TurnsCalculation.cpp` (seed formula, always implemented - previously mis-documented as a stub) / `src/core/magnetics/TurnsAndGapDesign.cpp` (Phase 1 convergence) | ✅ Implemented |
| Copper loss | `src/core/losses/CopperLoss.cpp`, `src/core/losses/LossEvaluation.cpp` | ✅ Implemented; real for most candidates now via a geometry-derived mean-length-per-turn estimate, `not_evaluated` only for cores whose upstream geometry doesn't support it (see DATA_FILES.md) |
| Core / high-freq losses | `src/core/losses/CoreLoss.cpp` (real Steinmetz formula, ripple-gated), `src/core/losses/HighFrequencyLosses.cpp` (not implemented) | ✅ Core loss: `Evaluated` when the material has real coefficients for this frequency AND the request supplies `rippleCurrentPeakToPeakA`; `not_evaluated` otherwise (never approximated from peak flux). High-frequency: `not_evaluated` (not implemented in Phase 1) |
| Fill factor check | `WindingFitValidation` (`DesignValidation.cpp`) | ✅ Implemented and compiled (`VALIDATION_SOURCES` is no longer empty) |
| Flux density check | `PeakFluxValidation`/`SaturationValidation` (`DesignValidation.cpp`) | ✅ Implemented; prefers a material's measured `BmaxT` (real data for all 32 materials), falling back to `DesignRules.defaultFluxDensityLimitT` and flagging `usedDefaultLimit: true` only then |