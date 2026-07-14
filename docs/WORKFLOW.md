# AIMagnetics Design Workflow

This document explains each stage of the inductor sizing process, what parameters matter, and which stages are currently implemented.

## Overview

The tool automates inductor design using McLyman's area-product method. It follows a 4-stage pipeline. **Stages 1–3 are implemented and wired through the API. Stage 4 is not yet implemented** — the formulas below are the target design, documented ahead of the code so the math is settled before it's written.

---

## Stage 1: Material Selection — ✅ Implemented

**File:** `src/core/MaterialSelection.cpp`

**Purpose:** Choose the best magnetic material for the operating frequency.

**Input:**
- `switchingFreqHz` — how fast the circuit switches

**Output:**
- Material name (Powder Iron, Kool Mu, Ferrite 3C90, etc.)
- Optimal permeability (µ_opt)
- Reason + alternatives (pipe-separated string, e.g. `"Ferrite|Kool Mu"`)

**How it works:** loops through `materials.csv` and returns the first material whose `[MinFrequencyHz, MaxFrequencyHz)` range contains the input frequency.

**Not yet used:** `MaterialSelectionInput` also declares `inductanceH`, `peakCurrentA`, `allowableTempRiseC`, and `waveformFactor`, but the current implementation only reads `switchingFreqHz` — the others are accepted but ignored. Current waveform shape has no effect on material selection yet.

---

## Stage 2: Area Product (Ap) Calculation — ✅ Implemented

**File:** `src/core/AreaProduct.cpp`

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

**Note:** `windowUtilization`, `fluxDensityT`, and `currentDensityAPerCm2` are currently hard-coded by the caller (`python/routes/core_selection.py` sets Ku=0.4, Bmax=0.30 T, J=400 A/cm²) rather than looked up per-material from `materials.csv`'s `BmaxT` column — worth reconciling, since `materials.csv` already carries a `BmaxT` per material that isn't being read here yet.

---

## Stage 3: Core Selection — ✅ Implemented

**File:** `src/core/CoreSelection.cpp`

**Purpose:** Find a real, available inductor core from the database that satisfies the Ap requirement.

**Input:**
- `areaProduct` (from Stage 2), `peakCurrentA`, `recommendedMaterial` (from Stage 1)
- Cores loaded from `data/cores.csv`

**Output:**
- Core part number, material, `mu`, `al`, `ae` (mm²), `wa` (mm²), `le` (mm)

**Selection Logic (as implemented):**
```
For each core in database:
coreAp = (Ae_mm² × Wa_mm²) × 1e-4 // convert to cm⁴
if coreAp >= input.areaProduct × 0.95: // 5% safety margin
add to candidates

if candidates is empty:
fall back to the single largest-Ap core in the database (with a console warning)
else:
prefer candidates matching recommendedMaterial (if none match, keep all candidates)
pick the one with lowest estimated copper loss:
estimatedLossW = peakCurrentA² / (Ae × Wa × 0.01) // simplified heuristic
```

**Current limitation:** ranking is by this single loss heuristic only. `cores.csv` includes a `PartCost` column, but cost- and size-based ranking are not implemented yet — there is no `sort_by` option in the current API.

**Debug output:** prints candidate pass/fail and the final loss comparison to the console — useful for demoing, not yet surfaced in the API response.

---

## Stage 4: Turns, Losses, Thermal Validation — ❌ Not Implemented

**Files (all currently stubs — commented out or hard-coded to return 0):**
`src/core/TurnsCalculation.cpp`, `src/core/CopperLoss.cpp`, `src/core/CoreLoss.cpp`, `src/core/GapDesign.cpp`, `src/core/HighFrequencyLosses.cpp`

**Target formulas (documented here for reference — not yet coded):**

**Turns:**
```
N = √(L × 10⁸ / (µ × Ae)) (where µ = µ₀ × µᵣ)
```

**Copper Loss:**
```
R_wire = ρ × l / A_copper
P_cu = I_rms² × R_wire
```

**Core Loss** (material-dependent; from vendor datasheets):
```
P_core = K_f × f^a × B^b × Volume (typical: a≈1.5–2.0, b≈2.5–3.0)
```

**Target validation, once implemented:** total loss vs. temperature budget; turns count sanity check (< 5 or > 100 flagged as impractical).

**Illustrative hand-calculation (not tool output — Stage 4 isn't wired up yet):**

For the reference part i77006 (250 µH, Kool Mu, core 0077440A7, Ae=199 mm², Wa=427 mm²):
```
N ≈ √(250µ × 10⁸ / (26 × 199)) ≈ 14 turns (hand-calculated target; matches reference_designs.csv's 64 turns only loosely — reconcile before treating either as ground truth)
Copper loss, core loss: not yet calculable until CopperLoss.cpp / CoreLoss.cpp are implemented
```

---

## Test Scenarios

See `data/test_scenarios.csv` (7 scenarios defined). Each row currently must be checked by hand — there's no automated test runner yet:

1. **i77006_validation** — 250µH, 5A, 100kHz, 40°C → expects core 0077440A7, turns 14–20
2. **low_power_rf** — 100µH, 2A, 500kHz, 30°C → expects core 0054035
3. **buck_10A** — 470µH, 10A, 80kHz, 50°C → expects core 0055610
4. **budget_option** — 150µH, 3A, 120kHz, 60°C → expects core 0054003
5. **high_efficiency** — 220µH, 7A, 95kHz, 35°C → expects core 0077439
6. **low_freq_power** — 1000µH, 15A, 25kHz, 55°C → expects core 0055601
7. **thermal_constraint** — 180µH, 4A, 110kHz, 25°C → expects core 0054035L

Since Stage 4 isn't implemented, only the material + core selection columns (`ExpectedCore`) can currently be checked; `ExpectedTurns` can't be verified against tool output yet.

---

## Parameter Cheat Sheet

| Parameter | Units | Typical Range | Impact |
|---|---|---|---|
| Inductance (L) | µH | 100 – 1000 | Larger L → larger core, more turns |
| Peak Current (Ipk) | A | 1 – 30 | Higher I → hotter, larger core |
| Frequency | kHz | 25 – 1000 | Higher f → smaller core, ferrite better |
| Temp Rise (ΔT) | °C | 25 – 60 | Currently accepted as input but not yet checked against anything (Stage 4 pending) |
| Window Utilization (Ku) | – | 0.4 (hard-coded) | Not yet configurable per-request |
| Flux Density (Bmax) | T | 0.30 (hard-coded) | Not yet read from `materials.csv`'s per-material `BmaxT` |
| Current Density (J) | A/cm² | 400 (hard-coded) | Not yet configurable per-request |

---

## Common Issues & Fixes

| Issue | Likely Cause | Fix |
|---|---|---|
| No cores meet Ap | Peak current or L too high for the database's largest core | Tool currently falls back to the largest available core rather than erroring — check the console warning |
| Material always the same regardless of input | Frequency range in `materials.csv` misconfigured | Check `materials.csv`'s `MinFrequencyHz`/`MaxFrequencyHz` |
| Core doesn't change between runs | Rebuild not picked up | Re-run the CMake build step (`--target magnetics_cpp`), then restart uvicorn |
