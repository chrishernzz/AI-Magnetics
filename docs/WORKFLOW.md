# AIMagnetics Design Workflow
This document explains each stage of the inductor sizing process and what parameters matter.

## Overview
The tool automates inductor design using McLyman's area-product method. It follows a 4-stage pipeline:

---
## Stage 1: Material Selection
**Purpose:** Choose the best magnetic material for the operating frequency.
**Input:**
- `switchingFrequencyKHz` — how fast the circuit switches
**Output:**
- Material name (Powder Iron, Kool Mu, Ferrite 3C90, etc.)
- Optimal permeability (µ_opt)
- Alternatives if chosen material is unavailable
**Why It Matters:**
Different materials have different loss curves. At 100 kHz, Kool Mu is better than Powder Iron (lower loss). But at 25 kHz, Powder Iron is better (can handle higher current density without saturation).
**File:** `src/core/MaterialSelection.cpp`

---
## Stage 2: Area Product (Ap) Calculation
**Purpose:** Determine the minimum physical size core needed to store energy without overheating.
**Input:**
- `inductanceUH` — desired inductance (from circuit designer)
- `peakCurrentA` — max current through inductor
- `allowableTempRiseC` — how hot can the core/wire get?
**Output:**
- `Ap` value in cm⁴
**The Formula:**
E_max = 0.5 × L × Ipk² (energy stored)
Ap = 2 × E_max × 10⁴ / (Ku × Bmax × J)
Where:
- `Ku` = window utilization (0.4 = 40% of core window has copper)
- `Bmax` = max flux density (material property; higher = smaller core)
- `J` = current density (400 A/cm²; higher J = hotter wire)

**Example:**
L = 250 µH, Ipk = 5A, ΔT = 40°C
E_max = 0.5 × 250µ × 25 = 3.125 mJ
Ap ≈ 3 cm⁴ (core must satisfy this minimum)

**Why It Matters:**
This is the *size requirement*. If Ap is too small relative to core size, the design will overheat. If Ap is too large, you're picking an unnecessarily big (expensive, heavy) core.
**File:** `src/core/AreaProduct.cpp`

---
## Stage 3: Core Selection
**Purpose:** Find a real, available inductor core from the database that satisfies the Ap requirement.
**Input:**
- `areaProduct` — the Ap value from Stage 2
- Available cores from `data/cores.csv`
**Output:**
- Core part number (e.g., 0077440A7)
- Core geometry: Ae (cross-section), Wa (window area), Le (magnetic path length)
- Material, permeability (µ), inductance index (AL)
**Selection Logic:**
For each core in database:
    If core.Ae × core.Wa >= Ap (with 5% safety margin):
    Add to candidates
Sort candidates by:
Efficiency (lowest copper + core loss)
Cost (cheapest option)
Size (smallest core)

**Why It Matters:**
Multiple cores might meet the Ap requirement. We need to pick the best one based on what matters: efficiency, cost, or size.
**File:** `src/core/CoreSelection.cpp`

---
## Stage 4: Design Validation & Loss Calculation
**Purpose:** Confirm the selected core will work, and calculate expected losses.
**Input:**
- Selected core (part number, Ae, Wa, material)
- Desired inductance L
- Peak current Ipk
- Switching frequency f
**Output:**
- Turns count (N) to achieve desired L
- Copper loss (I²R heating in wire)
- Core loss (hysteresis + eddy current loss)
- Total loss (Cu + Core)
- Temperature rise (validates against ΔT limit)
**Formulas:**
**Turns:**
N = √(L × 10⁸ / (µ × Ae))
(where µ = µ₀ × µᵣ)
**Copper Loss:**
R_wire = ρ × l / A_copper
P_cu = I_rms² × R
**Core Loss** (material-dependent; from vendor datasheets):
P_core = K_f × f^a × B^b × Volume
(typical: a≈1.5-2.0, b≈2.5-3.0)
**Why It Matters:**
Validates the whole design. If losses exceed budget, try a larger core or different material. If turns count is > 100 or < 5, the design is impractical.
**File:** `src/core/CopperLoss.cpp`, `src/core/CoreLoss.cpp`

---
## Test Scenarios
See `data/test_scenarios.csv` for example inputs and expected outputs. Each scenario tests a different aspect of the workflow:
1. **i77006_validation** — Validates against real IntelliPower spec
2. **low_power_rf** — Tests high-frequency material selection
3. **buck_10A** — Tests high-current Powder Iron core
4. **budget_option** — Tests cost-driven ranking
5. **high_efficiency** — Tests efficiency-driven ranking
6. **low_freq_power** — Tests very low-frequency design
7. **thermal_constraint** — Tests tight temperature limits

---
## Parameter Cheat Sheet

| Parameter | Units | Typical Range | Impact |
|-----------|-------|---------------|--------|
| Inductance (L) | µH | 100 - 1000 | Larger L → larger core, more turns |
| Peak Current (Ipk) | A | 1 - 30 | Higher I → hotter, larger core, lower Z |
| Frequency | kHz | 25 - 1000 | Higher f → smaller core, ferrite better |
| Temp Rise (ΔT) | °C | 25 - 60 | Tighter ΔT → smaller J → larger core |
| Window Utilization (Ku) | - | 0.3 - 0.5 | Higher Ku → more copper, hotter |
| Flux Density (Bmax) | T | 0.3 - 1.6 | Higher Bmax → smaller core, saturation risk |
| Current Density (J) | A/cm² | 200 - 600 | Higher J → faster, hotter wire |

---
## Example Walkthrough: i77006 (250 µH inductor)
**Step 1: Material Selection**
- Frequency = 100 kHz
- Database recommends: Kool Mu (50-250 kHz range)
**Step 2: Area Product**
- L = 250 µH, Ipk = 5A, ΔT = 40°C
- E_max = 3.125 mJ
- Ap ≈ 3 cm⁴
**Step 3: Core Selection**
- Search database for cores where Ae × Wa ≥ 3 cm⁴
- Candidates: 0077440A7 (best efficiency), 0077439 (cheapest), 0054044 (balance)
- Select 0077440A7 (Kool Mu, Ae=199, Wa=427)
**Step 4: Validation**
- Turns: N = √(250µ × 10⁸ / (26 × 199)) ≈ 14 turns
- Copper Loss: ~2.3W (estimate from wire gauge, length, DC resistance)
- Core Loss: ~0.8W (from vendor loss curves)
- Total: ~3.1W (acceptable for 40°C rise)

**Output Spec:** IntelliPower i77006 (250 µH, Kool Mu A7 core, 14-20 turns, #13 wire)

---
## Common Issues & Fixes
| Issue | Likely Cause | Fix |
|-------|--------------|-----|
| Ap too large (> 50 cm⁴) | Peak current too high or L too large | Reduce peak current spec, accept ripple |
| No cores satisfy Ap | Material selection wrong | Try higher-frequency material (lower loss) |
| Turns count too high (>100) | Inductance too large | Use higher-µ material or accept larger core |
| Turns count too low (<5) | Inductance too small | Use lower-µ material or accept smaller core |
| Temperature rise exceeds limit | Current density too high | Use larger wire, pick lower-loss core |
| Design too expensive | Selected core has premium material | Try alternative with lower cost |
---