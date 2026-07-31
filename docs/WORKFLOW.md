# AIMagnetics Design Workflow

This document explains each stage of the inductor sizing process, what parameters matter, and which stages are currently implemented.

## Overview

The tool automates inductor design using McLyman's area-product method. Stages 1-3 below explain the underlying frequency-matching and Ap formulas - originally implemented as a single-pick in `MaterialSelection.cpp`/`CoreSelection.cpp`, now superseded by `src/core/sizing/MaterialEvaluation.cpp`/`CoreEvaluation.cpp`, which apply the same formulas but return every compatible candidate instead of just the first. The formulas themselves didn't change; only the candidate-selection behavior did. The canonical Phase 1 pipeline behind `POST /inductor-design` runs candidate evaluation, turns/gap design, magnetic validation, winding design, loss evaluation, and a real loss-based ranking end to end - see Stage 4+ below, which is **implemented**, not a future plan. Thermal rise remains data-gapped and is documented ahead of the data existing.

---

## Stage 0 (optional): Buck Converter Requirement Derivation — ✅ Implemented

**File:** `src/backend/services/BuckElectricalSolver.cpp`, `POST /topology-design/buck`

**Purpose:** For engineers who know their Buck converter's operating point
but not their inductor's requirements yet. Everything below (Stage 1
onward) still expects a direct `InductorDesignRequest` - this stage exists
purely to produce that same struct from converter-level inputs, so it is
not a second pipeline. Skip this stage entirely if you already have your
inductor's requirements.

**Input:** `vinMinV`, `vinMaxV`, `voutV`, `ioutA`, `switchingFreqKHz`,
`rippleCurrentPercent` (target ripple as % of `ioutA`), plus
`ambientTemperatureC`/`allowableTempRiseC`/`inductanceTolerancePercent`
(passed through unchanged).

**Output:** `inductanceUH`, `peakCurrentA`, `switchingFreqKHz`,
`averageCurrentA`, `rippleCurrentPeakToPeakA` - i.e. an
`InductorDesignRequest`. `rmsCurrentA` is left unset deliberately.

**The Formulas** (standard buck converter inductor sizing, ideal - no
diode/switch drop modeling):
```
D (duty cycle) = Vout / Vin
ripple_A = Iout × (ripple% / 100)
L = (Vin - Vout) × D / (fsw × ripple_A)
Ipeak = Iout + ripple_A / 2
```

**Worst-case point:** all of the above is evaluated at `Vin = vinMaxV`,
not swept across `vinMinV..vinMaxV`. A buck converter's inductor ripple
current increases as Vin increases (`ripple = Vout(1 - Vout/Vin) / (fsw·L)`
grows toward `Vout/(fsw·L)` as Vin→∞), so sizing L at `Vin_max` is the
correct worst case for ripple and keeps it at or under target across the
whole range. Evaluating every quantity (ripple, peak current, flux,
thermal) at whichever Vin point is actually worst *for that quantity* -
which need not be `Vin_max` for all of them - is real, correct practice
but out of scope for V1; this is a documented simplification, not a
silent one.

**Why RMS current is not computed here:** `averageCurrentA` (= `Iout`) and
`rippleCurrentPeakToPeakA` are handed to the same
`RequirementDerivationService::derive()` that Stage 1 onward already runs
for direct entry, which derives RMS current from them using the
triangular-ripple formula (`Irms = sqrt(Iavg^2 + ripple^2/12)`) documented
in Stage 1's section below. There is exactly one RMS-derivation formula in
the codebase regardless of which mode produced the request.

**Example:** Vin = 36-60V, Vout = 12V, Iout = 40A, fsw = 500kHz, ripple = 20%
→ worst case at Vin=60V: D = 0.2, ripple = 8A, L = 2.4µH, Ipeak = 44A
(this is the hand-calculated case `tests/python/test_buck_electrical_solver.py` checks against).
For the same request carried all the way through core selection, turns/gap,
validation, winding, and losses, see [WORKED_EXAMPLE_MODE1.md](WORKED_EXAMPLE_MODE1.md).

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

**Note:** `windowUtilization`, `fluxDensityT`, and `currentDensityAPerCm2` are sourced from `DesignRules::phase1Default()` (Ku=0.4, Bmax=0.30 T, J=400 A/cm²) - a named C++ ruleset, not a hard-coded Python constant (spec section 7; see `src/rules/DesignRules.cpp`). The Phase 1 pipeline's `PeakFluxValidation`/`SaturationValidation` checks prefer a material-specific `BmaxT` over this default when one exists - `data/real_materials.csv`'s `BmaxT` is real, material-specific data for all 81 materials, so the default is now only a fallback for a material with no measured value, and every check that uses the default flags `usesDefaultAssumption: true` rather than presenting 0.30 T as a material fact.

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

**Files:** `src/core/magnetics/GapDesign.cpp`, `src/core/magnetics/TurnsAndGapDesign.cpp`, `src/validation/DesignValidation.cpp`, `src/core/winding/WindingDesign.cpp`, `src/core/losses/LossEvaluation.cpp` (calling `src/core/losses/CopperLoss.cpp`/`src/core/losses/CoreLoss.cpp`), `src/core/losses/SkinDepthRisk.cpp`, `src/core/thermal/ThermalEvaluation.cpp`, `src/validation/RecommendationStatus.cpp`, orchestrated by `src/backend/services/InductorDesignService.cpp` behind `POST /inductor-design`.

**Turns and gap (implemented, iterated together):**
```
AL0(nH/turn^2) = 0.4*pi * muR * Ae_cm2 / Le_cm * 10                       (ungapped catalog AL)
lg_cm          = 0.4*pi * N^2 * Ae_cm2 * 10 / L_target_nH - Le_cm/muR      (required gap for N turns)
AL_eff(nH/turn^2) = 0.4*pi * Ae_cm2 * 10 / (Le_cm/muR + gapCm)             (gapped AL)
```
`src/core/magnetics/TurnsAndGapDesign.cpp` seeds turns from the existing `src/core/magnetics/TurnsCalculation.cpp` formula (`N = round(sqrt(L/AL))` against the ungapped AL - this file was previously mis-documented elsewhere as a stub; it has always been implemented), then iterates gap -> effective AL -> turns until the integer turns count stabilizes (2-4 iterations typical) or is rejected (gap exceeds 40% of the core's magnetic path length, or no convergence within 15 iterations). Verified against `data/real_cores.csv`'s `E100/60/28-3C90` row to <0.03% (see `tests/cpp/EngineTests.cpp`).

**Magnetic validation (six named checks, `DesignValidation.cpp`):** InductanceValidation, PeakFluxValidation (`Bpk = L*Ipk/(N*Ae)` vs. the applicable flux limit), SaturationValidation (margin vs. `DesignRules.minimumSaturationMarginPercent`), WindingFitValidation, CurrentDensityValidation, ThermalValidation. Every failed check is reported, not just the first.

**Winding design (`src/core/winding/WindingDesign.cpp`):** required conductor area from RMS current and `DesignRules.allowableCurrentDensityAperCm2`, AWG gauge selection (`src/data/AwgTable.h`, standard NEMA MW1000 reference geometry) with parallel strands when a single strand would be impractically thick, raw copper fill factor, current density - all computed. A separate, realistic **physical window fill** (insulation build-up, achievable packing density, bobbin wall thickness, margin/lead-exit clearance - all `DesignRules` estimates) is what `WindingFitValidation` actually gates on; the raw copper-only figure stays as an informational field. DCR (now including lead/routing/connection resistance, not just core-winding resistance) and total wire length are computed from `CoreCandidate.mltMm` (a real-geometry estimate, `data/real_cores.csv`'s `Mlt` column) when it's available, `not_evaluated` for the subset of cores whose upstream geometry doesn't support that estimate. See FORMULAS.md section 6.

**Copper Loss (`src/core/losses/CopperLoss.cpp`, implemented):**
```
P_cu = I_rms^2 * DCR
```
Computed whenever `WindingDesign` produced a real DCR - real for most candidates now, `not_evaluated` only when the core's MLT estimate is unavailable.

**Core Loss (`src/core/losses/CoreLoss.cpp`, real Steinmetz formula, ripple-gated):**
```
Pv (W/m^3) = k * f^alpha * B^beta
Bswing (T) = calculatedInductanceH * ripplePeakToPeakA / (turns * Ae_m2)
```
Real for candidates whose material has coefficients in `data/real_core_loss_coefficients.csv` (25 of 81) AND whose request supplied `rippleCurrentPeakToPeakA` - `MaterialCandidate.hasCoreLossData` now reflects real coefficient availability, not the retired `CuLossFactor` field. No ripple current means no flux-density swing to compute Bswing from, so `losses.coreLossStatus` stays `not_evaluated` rather than approximating it from peak flux. Coefficients' temperature-correction terms (ct0/ct1/ct2) are not yet applied.

**Skin-depth AC-loss risk (`src/core/losses/SkinDepthRisk.cpp`, renamed from the dead `HighFrequencyLosses.cpp` stub):** a real, qualitative Low/Moderate/High risk level based on the selected strand's bare radius vs. the classical skin depth at the switching frequency - never a watts figure (`acLossWattsStatus` is permanently `not_evaluated`, since no AC-loss watts model exists). Named explicitly what it evaluates (single-strand skin effect) and what it doesn't (bundle proximity effect, proximity to the air gap). See FORMULAS.md section 11.

**Thermal evaluation (`src/core/thermal/ThermalEvaluation.cpp`):** a real iterative convergence loop (temp → hot DCR → copper loss → temp rise → repeat, mirroring the turns/gap loop above) now runs, using a size-aware thermal resistance estimated from each candidate's own real Ae/Le core geometry via Newton's law of cooling (falls back to the flat `DesignRules.defaultThermalResistanceCPerW` only when that geometry is unavailable) - neither is per-core measured data. Caps at `PreliminaryThermalEstimate` - never a "fully evaluated" value exists. Reports `not_evaluated` when winding DCR geometry is unknown, or when the loop's real positive-feedback iteration diverges rather than converges (a genuine possibility for high-current, low-DCR designs, not just a numerical edge case). See FORMULAS.md section 10.

**3-tier recommendation (`src/validation/RecommendationStatus.cpp`):** `Pass` / `ConditionalPass` / `Reject` replaces the old frontend-only "Recommended" sugar. `Reject` always mirrors the six-check pass/fail decision above. No real request reaches `Pass` today, since `ThermalValidation` always flags its result as a preliminary estimate. See FORMULAS.md section 12.

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
| Temp Rise (ΔT) | °C | 25 – 60 | Checked by ThermalValidation against a real iterative loop's converged result - `PreliminaryThermalEstimate` at best (never fully evaluated), `not_evaluated` if DCR geometry is unknown or the loop diverges |
| Window Utilization (Ku) | – | 0.4, from `DesignRules::phase1Default()` | Not yet configurable per-request |
| Flux Density (Bmax) | T | 0.30 default, from `DesignRules::phase1Default()` | Used unless a material carries its own `BmaxT` (real, material-specific data in `real_materials.csv` for all 81 materials) |
| Current Density (J) | A/cm² | 400, from `DesignRules::phase1Default()` | Not yet configurable per-request |

---

## Common Issues & Fixes

| Issue | Likely Cause | Fix |
|---|---|---|
| `status: "no_feasible_design"`, no cores meet Ap | Peak current or L too high for the database's largest core | `POST /inductor-design` returns `requiredAreaProductCm4`/`largestAvailableAreaProductCm4` directly in the response - no console-only warning, no silent oversized fallback |
| Material always the same regardless of input | Frequency ranges misconfigured, or filters in `magnetics_data.py` too narrow | Check the loaded materials' `MinFrequencyHz`/`MaxFrequencyHz`; `/inductor-design` returns every frequency-compatible material as a candidate, not just one |