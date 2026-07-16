# AIMagnetics Test Case: Heavy-Duty Powder Iron Design

**Historical worked example - not reproducible against the live app anymore.** This walks through Stages 1-3 (Material Selection, Area Product, Core Selection) using the math as it worked when this document was written, back when `/material-selection`/`/calculate`/`/core-selection` and `CoreSelection.cpp`'s single-pick logic existed. Those endpoints and that file have since been deleted (see [ARCHITECTURE.md](ARCHITECTURE.md)) - the underlying Ap formula is unchanged and still correct as a hand-check, but you can no longer `curl` these exact steps or see this exact debug output, since there's only one endpoint now (`POST /inductor-design`) and it returns every compatible candidate rather than one core selected by this document's loss heuristic. For a live, automated equivalent, see `tests/python/test_reference_designs.py` and [WORKFLOW.md](WORKFLOW.md)'s "Stage 4+" section.

## Test 3: Heavy-Duty Design (Primary Validation Test)

This is the most important test. It validates that:
- ✅ Material selection picks the right material for frequency
- ✅ Area Product calculation is correct
- ✅ Core filtering respects material recommendations
- ✅ Energy calculation works (not 0.00 mJ)

## Input Values

Enter these exact values in the browser form:
- Inductance (L): 470 µH
- Peak Current (Ipk): 10 A
- Switching Frequency: 80 kHz
- Temperature Rise: 50 °C

## Expected Output

After clicking "Generate Recommendation," you should see:
- Material Recommendation: Powder Iron
- Core Recommendation: 0055500F or 0077443
- µ: 90 or 26
- AL: 40 or 59 nH/T²
- Ae: 249 or 319 mm²
- Wa: 399 or 599 mm²
- Le: 161 or 135 mm
- Stored Energy: 23.5 mJ
- Area Product (Ap): 9.79 cm⁴ (approx)

## Why These Results?

### STAGE 1: Material Selection

At 80 kHz:
- Powder Iron: 1 kHz to 100 kHz ← 80 kHz is here
- Kool Mu: 50 kHz to 250 kHz ← also matches, but not the first match
- Ferrite 3C90: 50 kHz to 250 kHz
- High Frequency: 250 kHz to 10 MHz ← too high

**Decision:** Powder Iron. (Note: `MaterialSelection.cpp` returns the *first* matching material in `materials.csv` file order, so this depends on Powder Iron's row preceding the others — not a "best of all matches" comparison.)

**System Output:** Material Recommendation: Powder Iron ✅

### STAGE 2: Area Product Calculation

```
E = 0.5 × L × Ipk²
E = 0.5 × 470×10⁻⁶ H × (10 A)²
E = 23.5 × 10⁻³ J = 23.5 mJ

Ap = (2 × E_max × 10⁴) / (Ku × Bmax × J)
Ku = 0.4, Bmax = 0.30 T, J = 400 A/cm² (all currently hard-coded in the route, not read from materials.csv)

Ap = (2 × 23.5×10⁻³ × 10⁴) / (0.4 × 0.30 × 400)
Ap = 470 / 48
Ap = 9.79 cm⁴
```

Safety margin: 9.79 × 0.95 = 9.30 cm⁴ minimum.

**System Output:** Stored Energy: 23.5 mJ ✅ · Area Product: 9.79 cm⁴ ✅

### STAGE 3: Core Database Filtering

| Core | Material | Ae(mm²) | Wa(mm²) | Ap(cm⁴) | Status |
|---|---|---|---|---|---|
| 0077440A7 | Kool Mu | 199 | 427 | 8.50 | ❌ too small |
| 0055500 | Powder Iron | 74 | 58 | 0.43 | ❌ too small |
| 0055610 | Powder Iron | 159 | 299 | 4.75 | ❌ too small |
| 0077441 | Kool Mu | 279 | 600 | 16.74 | ✅ passes, wrong material |
| 0055601 | Powder Iron | 249 | 316 | 7.87 | ❌ too small |
| 0055500F | Powder Iron | 249 | 399 | 9.95 | ✅ passes, Powder Iron |
| 0077442 | Kool Mu | 359 | 800 | 28.72 | ✅ passes, wrong material |
| 0077443 | Powder Iron | 319 | 599 | 19.11 | ✅ passes, Powder Iron |

**Step 1 — filter by Ap ≥ 9.30 cm⁴:** 0077441, 0055500F, 0077442, 0077443
**Step 2 — filter by material = Powder Iron:** 0055500F, 0077443
**Step 3 — rank by the loss heuristic** (`loss = Ipk² / (Ae × Wa × 0.01)`):
```
0055500F: loss = 100 / (249 × 399 × 0.01) = 0.1007
0077443: loss = 100 / (319 × 599 × 0.01) = 0.0523 ← lower, wins
```

**Winner:** 0077443

## How to Verify Results (historical - see the framing note at the top)

**In browser, back when these endpoints existed:** material = Powder Iron, core = 0055500F or 0077443, energy ≈ 23.5 mJ, Ap ≈ 9.79 cm⁴.

**In terminal, back when `CoreSelection.cpp` existed (this file is now deleted, so this output can no longer be reproduced):**
```
=== CORE SELECTION DEBUG ===
Input Ap requirement: 9.79167 cm⁴
Recommended material: Powder Iron

Core 0077440A7 (Kool Mu): Ap=8.4973 → FAIL
...
Core 0055500F (Powder Iron): Ap=9.95 → PASS
...
Core 0077443 (Powder Iron): Ap=19.1081 → PASS

Total candidates: 2
Filtered by material "Powder Iron": 2 cores

Loss comparison:
0055500F: loss=0.100653
0077443: loss=0.0523338 ← NEW BEST

Selected: 0077443 (Powder Iron)
```

**To verify the equivalent today:** POST the same inputs to `/inductor-design` and inspect `activeRules`/`candidates` in the JSON response, or run `tests/python/test_reference_designs.py`.

## Troubleshooting

| Issue | Cause | Fix |
|---|---|---|
| Material is "Kool Mu" (wrong) | Frequency ranges in `materials.csv` misconfigured, or file order changed | Check Powder Iron's range covers 1000–100000 Hz and precedes Kool Mu in the file |
| Energy shows 0.00 mJ | Input not reaching the C++ engine correctly | Check the payload sent by `app.js`'s `buildPayload()` |
| Core is always the largest one | No core met Ap | This legacy endpoint no longer silently falls back — it returns `partNumber: "No compatible core found"` instead. If you're on `/inductor-design`, check for `status: "no_feasible_design"` and `requiredAreaProductCm4`/`largestAvailableAreaProductCm4` in the response |
| Core doesn't change between tests | Extension not rebuilt / server not restarted | Re-run the CMake build for `magnetics_cpp`, restart uvicorn |

## Technical Definitions

| Term | Means | Units |
|---|---|---|
| L (Inductance) | Energy storage capacity | µH |
| Ipk (Peak Current) | Maximum current through inductor | A |
| Frequency | How fast circuit switches | kHz |
| ΔT (Temp Rise) | Allowed temperature increase (not yet validated against) | °C |
| Ae | Core cross-section area | mm² |
| Wa | Window area for copper wire | mm² |
| Ap (Area Product) | Size requirement (Ae × Wa, converted to cm⁴) | cm⁴ |
| E (Stored Energy) | Power being handled | mJ |