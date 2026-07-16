# Requirements traceability

Maps each input/output specified by your boss to the file that owns it, and whether it's actually implemented yet.

| Requirement (from boss) | Owned by | Status |
|---|---|---|
| L (given) | `AreaProduct.cpp` · `TurnsCalculation.cpp`/`TurnsAndGapDesign.cpp` | ✅ used in Area Product and in the Phase 1 turns/gap convergence loop |
| Peak current | `AreaProduct.cpp` · `DesignValidation.cpp` (PeakFluxValidation/SaturationValidation) | ✅ used in Area Product and peak-flux/saturation checks - never used for RMS-dependent checks |
| RMS current | `RequirementDerivationService.cpp` (direct, or derived from `averageCurrentA`+`rippleCurrentPeakToPeakA` for triangular ripple) · `WindingDesign.cpp` · `CopperLoss.cpp` | ✅ required/derivable input, used for winding sizing and DC copper loss; never inferred from peak current |
| Current waveform | Not tracked as its own field - superseded by `InductorDesignRequest.rippleCurrentPeakToPeakA` (see RMS current row) | ⚠️ Only triangular ripple is modeled; the old `waveformFactor` field (on the now-removed `MaterialSelectionInput`) was accepted but never read by anything, so nothing is lost |
| Switching frequency | `MaterialEvaluation.cpp` · `AreaProduct.cpp` · `CoreLoss.cpp` (gated on data) | ✅ used in material candidate evaluation and Area Product; Core Loss implemented but gated on `hasCoreLossData` (never true with today's data) |
| Ambient temperature | `InductorDesignRequest.ambientTemperatureC` · `ThermalEvaluation.cpp` | ⚠️ Accepted and threaded through; `ThermalEvaluation` always reports `not_evaluated` - no thermal model/data yet |
| Allowable temp rise | `InductorDesignRequest.allowableTempRiseC` · `ThermalValidation` (`DesignValidation.cpp`) | ⚠️ Real check exists and is called; always reports `not_evaluated` (never an assumed pass) pending thermal model/data |
| Ap / core selection (McLyman p.63) | `AreaProduct.cpp`, `CoreEvaluation.cpp`, `data/real_cores.csv` | ✅ Implemented - Phase 1 path never falls back to an oversized core; returns `no_feasible_design` instead |
| Gapped core branch | `GapDesign.cpp`, `TurnsAndGapDesign.cpp` | ✅ Implemented - series-reluctance model, iterated with turns until convergence |
| Turns | `TurnsCalculation.cpp` (seed formula, always implemented - previously mis-documented as a stub) / `TurnsAndGapDesign.cpp` (Phase 1 convergence) | ✅ Implemented |
| Copper loss | `CopperLoss.cpp`, `LossEvaluation.cpp` | ✅ Implemented; `not_evaluated` today because DCR is blocked on missing mean-length-per-turn data (see DATA_FILES.md) |
| Core / high-freq losses | `CoreLoss.cpp` (implemented, gated on data), `HighFrequencyLosses.cpp` (not implemented) | ⚠️ Core loss: `not_evaluated` (no material coefficients in the data). High-frequency: `not_evaluated` (not implemented in Phase 1) |
| Fill factor check | `WindingFitValidation` (`DesignValidation.cpp`) | ✅ Implemented and compiled (`VALIDATION_SOURCES` is no longer empty) |
| Flux density check | `PeakFluxValidation`/`SaturationValidation` (`DesignValidation.cpp`) | ✅ Implemented; uses `DesignRules.defaultFluxDensityLimitT` since no material carries a measured `BmaxT` yet, and flags `usedDefaultLimit: true` when it does |