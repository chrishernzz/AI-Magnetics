# AIMagnetics Design Workflow

This document explains each stage of the inductor sizing process, what parameters matter, and which stages are currently implemented.

## Overview

The tool automates inductor design using McLyman's area-product method. Stages 1-3 below explain the underlying frequency-matching and Ap formulas - originally implemented as a single-pick in `MaterialSelection.cpp`/`CoreSelection.cpp`, now superseded by `src/core/sizing/MaterialEvaluation.cpp`/`CoreEvaluation.cpp`, which apply the same formulas but return every compatible candidate instead of just the first. The formulas themselves didn't change; only the candidate-selection behavior did. The canonical Phase 1 pipeline behind `POST /inductor-design` runs candidate evaluation, turns/gap design, magnetic validation, winding design, and loss evaluation end to end - see Stage 4+ below, which is **implemented**, not a future plan. Formulas for what remains data-gapped (core loss, thermal) are still documented ahead of the data existing.

---

## Stage 1: Material Selection — ✅ Implemented

**File:** `src/core/sizing/MaterialEvaluation.cpp` (`findSuitableMaterials()`). Originally a single-pick in `MaterialSelection.cpp`, which has since been removed - the frequency-matching logic below is unchanged, but it now returns every frequency-compatible material as its own candidate instead of just the first match.

**Purpose:** Choose the best magnetic material for the operating frequency.

**Input:**
- `switchingFreqHz` — how fast the circuit switches

**Output:**
- Material name (Powder Iron, Kool Mu, Ferrite 3C90, etc.)
- Optimal permeability (µ_opt)
- Reason + alternatives (pipe-separated string, e.g. `"Ferrite|Kool Mu"`)

**How it works:** loops through the loaded material list (`data/real_materials.csv` — see `docs/ARCHITECTURE.md` for how it got there) and returns the first material whose `[MinFrequencyHz, MaxFrequencyHz)` range contains the input frequency.

**Historical note:** the now-removed `MaterialSelectionInput` struct also declared `inductanceH`, `peakCurrentA`, `allowableTempRiseC`, and `waveformFactor`, but only `switchingFreqHz` was ever read from it - the others were accepted but ignored, and current waveform shape had no effect on material selection. That struct is gone along with the old single-stage endpoints (see `docs/ARCHITECTURE.md`).

---

## Stage 2: Area Product (Ap) Calculation — ✅ Implemented

**File:** `src/core/sizing/AreaProduct.cpp`

**Purpose:** Determine the minimum physical core size needed to store energy without overheating.

**Input:**
- `inductanceH`, `peakCurrentA`, `windowUtilization` (Ku), `fluxDensityT` (Bmax), `currentDensityAPerCm2` (J)

**Output:**
- `Ap` value in cm⁴
- Stored energy in J

**The Formula:**
```
E_max = 0.5 × L × Ipk²
Ap = 2 × E_max × 10⁴ / (Ku × Bmax × J)
```

**Example:** L = 250 µH, Ipk = 5A
```
E_max = 0.5 × 250µ × 25 = 3.125 mJ
Ap ≈ 3 cm⁴ (core must satisfy this minimum)
```

**Note:** `windowUtilization`, `fluxDensityT`, and `currentDensityAPerCm2` are sourced from `DesignRules::phase1Default()` (Ku=0.4, Bmax=0.30 T, J=400 A/cm²) - a named C++ ruleset, not a hard-coded Python constant (spec section 7; see `src/rules/DesignRules.cpp`). The Phase 1 pipeline's `PeakFluxValidation`/`SaturationValidation` checks prefer a material-specific `BmaxT` over this default when one exists - `data/real_materials.csv`'s `BmaxT` is real, material-specific data for all 32 materials, so the default is now only a fallback for a material with no measured value, and every check that uses the default flags `usedDefaultLimit: true` rather than presenting 0.30 T as a material fact.

---

## Stage 3: Core Selection — ✅ Implemented

**File:** `src/core/sizing/CoreEvaluation.cpp` (`findSuitableCores()`). Originally a single-pick in `CoreSelection.cpp`, which has since been removed - the Ap-matching logic below is unchanged, but it now returns every material-compatible core (each flagged `meetsAreaProduct`) instead of one pick, and never falls back to an oversized core.

**Purpose:** Find a real, available inductor core from the database that satisfies the Ap requirement.

**Input:**
- `areaProduct` (from Stage 2), `peakCurrentA`, `recommendedMaterial` (from Stage 1)
- Cores loaded from the real database at startup (`src/data/CoreDatabase.h`'s `setData()`/`load()`, populated by `python/services/magnetics_data.py` - the old CSV-reading `CoreDatabase.cpp` was fully dead code and has been deleted)

**Output:**
- Core part number, material, `mu`, `al`, `ae` (mm²), `wa` (mm²), `le` (mm)

**Selection Logic (as implemented in `findSuitableCores()`):**
```
For each core in database:
coreAp = (Ae_mm² × Wa_mm²) × 1e-4 // convert to cm⁴
materialCompatible = core.material is one of the compatible material candidates
meetsAreaProduct = coreAp >= requiredAreaProductCm4 × 0.95 // 5% safety margin

if materialCompatible:
add to candidates, flagged with its own areaProductCm4 and meetsAreaProduct
```

Every material-compatible core is returned - passing or not - so the
caller (`InductorDesignService`) can build an honest `no_feasible_design`
report (required vs. largest-available Ap) if nothing passes, instead of
the old behavior of silently substituting the largest core. Ranking
among passing candidates happens one stage later, after turns/gap,
validation, winding, and loss evaluation have all run - the old
loss-proxy heuristic (`peakCurrentA^2 / (Ae × Wa × 0.01)`) that used to
rank cores by size alone was removed along with `CoreSelection.cpp`, not
carried forward.

---

## Stage 4+: Turns/Gap, Magnetic Validation, Winding, Losses, Thermal — ✅ Implemented (data-gapped in places)

**Files:** `src/core/magnetics/GapDesign.cpp`, `src/core/magnetics/TurnsAndGapDesign.cpp`, `src/validation/DesignValidation.cpp`, `src/core/winding/WindingDesign.cpp`, `src/core/losses/LossEvaluation.cpp` (calling `src/core/losses/CopperLoss.cpp`/`src/core/losses/CoreLoss.cpp`/`src/core/losses/HighFrequencyLosses.cpp`), `src/core/thermal/ThermalEvaluation.cpp`, orchestrated by `src/backend/services/InductorDesignService.cpp` behind `POST /inductor-design`.

**Turns and gap (implemented, iterated together):**
```
AL0(nH/turn^2) = 0.4*pi * muR * Ae_cm2 / Le_cm * 10                       (ungapped catalog AL)
lg_cm          = 0.4*pi * N^2 * Ae_cm2 * 10 / L_target_nH - Le_cm/muR      (required gap for N turns)
AL_eff(nH/turn^2) = 0.4*pi * Ae_cm2 * 10 / (Le_cm/muR + gapCm)             (gapped AL)
```
`src/core/magnetics/TurnsAndGapDesign.cpp` seeds turns from the existing `src/core/magnetics/TurnsCalculation.cpp` formula (`N = round(sqrt(L/AL))` against the ungapped AL - this file was previously mis-documented elsewhere as a stub; it has always been implemented), then iterates gap -> effective AL -> turns until the integer turns count stabilizes (2-4 iterations typical) or is rejected (gap exceeds 40% of the core's magnetic path length, or no convergence within 15 iterations). Verified against `data/real_cores.csv`'s `E100/60/28-3C90` row to <0.03% (see `tests/cpp/EngineTests.cpp`).

**Magnetic validation (six named checks, `DesignValidation.cpp`):** InductanceValidation, PeakFluxValidation (`Bpk = L*Ipk/(N*Ae)` vs. the applicable flux limit), SaturationValidation (margin vs. `DesignRules.minimumSaturationMarginPercent`), WindingFitValidation, CurrentDensityValidation, ThermalValidation. Every failed check is reported, not just the first.

**Winding design (`src/core/winding/WindingDesign.cpp`):** required conductor area from RMS current and `DesignRules.allowableCurrentDensityAperCm2`, AWG gauge selection (`src/data/AwgTable.h`, standard NEMA MW1000 reference geometry) with parallel strands when a single strand would be impractically thick, fill factor, current density - all computed. DCR and total wire length are computed from `CoreCandidate.mltMm` (a real-geometry estimate, `data/real_cores.csv`'s `Mlt` column) when it's available, `not_evaluated` for the subset of cores whose upstream geometry doesn't support that estimate.

**Copper Loss (`src/core/losses/CopperLoss.cpp`, implemented):**
```
P_cu = I_rms^2 * DCR
```
Computed whenever `WindingDesign` produced a real DCR - real for most candidates now, `not_evaluated` only when the core's MLT estimate is unavailable.

**Core Loss (`src/core/losses/CoreLoss.cpp`, implemented but unused with real data):** a simplified, documented-as-non-Steinmetz loss-density model, gated on `MaterialCandidate.hasCoreLossData`. Real Steinmetz coefficients now exist in `data/real_core_loss_coefficients.csv` for 17 of 32 materials, but this module hasn't been updated to read that file or use the real formula yet - `losses.coreLossStatus` is still `not_evaluated` for every candidate.

**High-frequency (skin/proximity) loss:** not implemented in Phase 1 (`src/core/losses/HighFrequencyLosses.cpp` still returns 0.0) - always reported `not_evaluated`, never presented as a real 0 W result.

**Thermal evaluation (`src/core/thermal/ThermalEvaluation.cpp`):** always `not_evaluated` - no thermal-resistance model or surface-area data exists in either CSV yet.

**Turns count sanity, once real turns exist:** the tool doesn't flag turns < 5 or > 100 as impractical yet - only the six named validation checks above run.

**Worked example (real tool output via `POST /inductor-design`):**

For the reference part i77006 (250 µH target, peak 5 A, RMS 3.5 A, 100 kHz, 25°C ambient, 40°C allowed rise) run against the current Ferroxcube-only `data/real_cores.csv`/`data/real_materials.csv` snapshot: see `tests/python/test_reference_designs.py::test_reference_design_i77006` for the exact request and how to reproduce it. The snapshot's cores don't include an i77006-equivalent part (it's a Kool Mu/Powder Iron design; the bundled snapshot is Ferroxcube-only ferrite), so this test checks turns count feasibility rather than an exact core match - see `docs/DATA_FILES.md` for the underlying data coverage gap.

---

## Test Scenarios

See `data/test_scenarios.csv` (7 scenarios). These are now checked by an
automated runner: `pytest tests/python/test_reference_designs.py`.

1. **i77006_validation** — 250µH, 5A, 100kHz, 40°C → expects core 0077440A7, turns 14–20
2. **low_power_rf** — 100µH, 2A, 500kHz, 30°C → expects core 0054035
3. **buck_10A** — 470µH, 10A, 80kHz, 50°C → expects core 0055610
4. **budget_option** — 150µH, 3A, 120kHz, 60°C → expects core 0054003
5. **high_efficiency** — 220µH, 7A, 95kHz, 35°C → expects core 0077439
6. **low_freq_power** — 1000µH, 15A, 25kHz, 55°C → expects core 0055601
7. **thermal_constraint** — 180µH, 4A, 110kHz, 25°C → expects core 0054035L

A basic sanity check (`test_scenario_produces_a_feasible_or_honestly_infeasible_result`)
confirms every scenario runs without crashing and always returns a
well-formed `status`. **`ExpectedCore`/`ExpectedTurns` exact-match checks
are `xfail`-marked, not silently skipped** (`test_expected_core_and_turns_match_original_catalog`):
these values were calibrated against a Kool Mu/Powder Iron catalog whose
part numbers don't exist in `data/real_cores.csv` (Ferroxcube-only) - a
data-source mismatch between this fixture and the live core database, not
an engine bug. See [DATA_FILES.md](DATA_FILES.md).

---

## Parameter Cheat Sheet

| Parameter | Units | Typical Range | Impact |
|---|---|---|---|
| Inductance (L) | µH | 100 – 1000 | Larger L → larger core, more turns |
| Peak Current (Ipk) | A | 1 – 30 | Higher I → hotter, larger core |
| Frequency | kHz | 25 – 1000 | Higher f → smaller core, ferrite better |
| Temp Rise (ΔT) | °C | 25 – 60 | Checked by ThermalValidation - always `not_evaluated` today (no thermal model/data yet) |
| Window Utilization (Ku) | – | 0.4, from `DesignRules::phase1Default()` | Not yet configurable per-request |
| Flux Density (Bmax) | T | 0.30 default, from `DesignRules::phase1Default()` | Used unless a material carries its own `BmaxT` (real, material-specific data in `real_materials.csv` for all 32 materials) |
| Current Density (J) | A/cm² | 400, from `DesignRules::phase1Default()` | Not yet configurable per-request |

---

## Common Issues & Fixes

| Issue | Likely Cause | Fix |
|---|---|---|
| `status: "no_feasible_design"`, no cores meet Ap | Peak current or L too high for the database's largest core | `POST /inductor-design` returns `requiredAreaProductCm4`/`largestAvailableAreaProductCm4` directly in the response - no console-only warning, no silent oversized fallback |
| Material always the same regardless of input | Frequency ranges misconfigured, or filters in `magnetics_data.py` too narrow | Check the loaded materials' `MinFrequencyHz`/`MaxFrequencyHz`; `/inductor-design` returns every frequency-compatible material as a candidate, not just one |