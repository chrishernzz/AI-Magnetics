# Data Files Guide

This document explains the structure of CSV files in `data/` and how to add new entries.

---

## cores.csv

**Purpose:** Database of available inductor cores (part numbers, geometry, materials).
**Location:** `data/cores.csv`
**Used by:** `src/data/CoreDatabase.cpp`
**Currently:** 16 rows, all vendor "Magnetics" (Magnetics Inc.)

### Fields (actual header)

| Field | Unit | Example | Description |
|---|---|---|---|
| PartNumber | — | 0077440A7 | Supplier part number (unique key) |
| Material | — | Kool Mu | Material type (must match `materials.csv`'s `Name`) |
| Mu | — | 26 | Relative permeability |
| AL | nH/100T | 59 | Inductance index (nH per 100 turns) |
| Ae | **mm²** | 199 | Cross-sectional area of core (millimeters², not cm²) |
| Wa | **mm²** | 427 | Window area available for copper winding (millimeters², not cm²) |
| Le | **mm** | 107 | Magnetic path length (millimeters, not cm) |
| PartCost | $ | 8.50 | Unit cost — present in the data but not yet used in core-selection ranking |
| Vendor | — | Magnetics | Vendor name |
| MaxCurrent_A | A | 8 | Max rated current |
| MaxFreq_kHz | kHz | 200 | Max rated frequency |

### How to Add a New Core

1. Get the datasheet from the supplier (needs Ae, Wa, Le in mm; AL; material)
2. Open `data/cores.csv`
3. Add a new row matching the 11-field order above, e.g.:
```
0077441A7,Kool Mu,26,59,150,320,90,3.00,Magnetics,12,200
```
4. Verify:
- `Material` matches a `Name` in `materials.csv` exactly (case-sensitive)
- `Ae` and `Wa` are in mm² (not cm²) — this is what `CoreSelection.cpp` expects; it converts internally via `(Ae × Wa) × 1e-4` to get cm⁴
5. Rebuild the pybind11 extension (core data is loaded at runtime, so a C++ rebuild isn't strictly required for a data-only change — but restart the running `uvicorn` process so it re-reads the CSV)

### Calculation Tips

`CoreSelection.cpp`'s core-side area product: `Ap_cm4 = (Ae_mm² × Wa_mm²) × 1e-4`. This is a different Ap than the one computed by `AreaProduct.cpp` (which computes the *required* Ap from L, Ipk, Ku, Bmax, J) — the two are compared against each other in Stage 3, not the same calculation.

If a datasheet gives µ instead of AL:
```
AL ≈ 0.4π × µ₀ × µᵣ × (Ae / Le) × 10⁹ (nH/100T; µ₀ = 4π×10⁻⁷, Ae in cm², Le in cm — convert your mm values before using this formula)
```

---

## materials.csv

**Purpose:** Database of magnetic materials and their properties.
**Location:** `data/materials.csv`
**Used by:** `src/data/Materials.cpp`, `MaterialSelection.cpp`
**Currently:** 4 rows — Powder Iron, Kool Mu, Ferrite 3C90, High Frequency Ferrite

### Fields (actual header)

| Field | Unit | Example | Description |
|---|---|---|---|
| Name | — | Kool Mu | Material identifier (unique; must match `cores.csv`'s `Material`) |
| MuOpt | — | 26 | Optimal permeability for the frequency range |
| MinFrequencyHz | Hz | 50000 | Minimum operating frequency |
| MaxFrequencyHz | Hz | 250000 | Maximum operating frequency |
| Reason | — | "Balanced performance 50-250kHz..." | Why this material suits its range |
| Alternatives | — | `Ferrite\|Powder Iron` | Pipe-separated alternatives; passed through as a raw string by the API, not parsed into a list |
| BmaxT | T | 1.0 | Max flux density — **not currently read by `AreaProduct.cpp`**, which uses a hard-coded 0.30 T instead |
| CuLossFactor | — | 1.15 | Multiplier for AC copper loss — not yet consumed anywhere (Stage 4 is a stub) |

### Frequency Ranges (current data)
- **Powder Iron:** 1–100 kHz — reason text notes "good for buck inductors" specifically; worth genericizing if the tool is meant to be topology-agnostic
- **Kool Mu:** 50–250 kHz
- **Ferrite 3C90:** 50–250 kHz
- **High Frequency Ferrite:** 250 kHz–10 MHz

### How to Add a New Material

1. Get the datasheet (µ, Bmax, loss curves)
2. Open `data/materials.csv`
3. Add a new row matching the 8-field order above
4. Verify frequency range doesn't unintentionally overlap in a way that changes which material wins for a given frequency (`MaterialSelection.cpp` returns the *first* match in file order)
5. Restart the running app to pick up the new row

---

## reference_designs.csv

**Purpose:** Real reference part(s) used for manual validation.
**Location:** `data/reference_designs.csv`
**Currently:** 1 row (the IntelliPower `i77006`)

### Fields (actual header)

| Field | Example | Description |
|---|---|---|
| PartNumber | i77006 | Reference part identifier |
| InductanceUH | 250 | Inductance (µH) |
| Turns | 64 | Actual turns count on the real part |
| WireAWG | 13 | Actual wire gauge on the real part |
| Core | 0077440A7 | Core part number used |

**Note:** this is a much smaller schema than a full design package (no current, frequency, or loss columns) — it exists purely to sanity-check core/turns selection against one known real part, documented in `TESTRESULTSMEAN.md`.

### How to Use
Add more real designs here as they become available — each new row is another point to validate the tool's core selection (and eventually turns calculation) against.

---

## test_scenarios.csv

**Purpose:** Test cases for validating the design algorithm, checked manually today (no automated runner exists yet).
**Location:** `data/test_scenarios.csv`
**Currently:** 7 rows

### Fields (actual header)

| Field | Example | Description |
|---|---|---|
| Name | i77006_validation | Scenario identifier |
| InductanceUH | 250 | Input: inductance |
| PeakCurrentA | 5 | Input: peak current |
| FrequencyKHz | 100 | Input: switching frequency |
| AllowedTempRiseC | 40 | Input: allowable temperature rise |
| ExpectedCore | 0077440A7 | Expected core part number |
| ExpectedTurns | 14-20 | Expected turns range — **can't be checked yet**, Stage 4 isn't implemented |
| TestDescription | "Real IntelliPower spec; should match" | Why this scenario matters |

### How to Run Tests (current process)
1. Enter each row's inputs into the web UI (or `curl` the endpoints directly)
2. Compare the returned `material` and `partNumber` against `ExpectedCore` / material implied by the description
3. `ExpectedTurns` can't be verified yet — there's no turns output until Stage 4 is implemented

### Adding Test Cases
When you find a bug or edge case:
1. Add a row with the failing scenario
2. Fix the code
3. Re-check the scenario by hand
4. Keep the row for future regression checking (once an automated runner exists)

---

## CSV Format Rules
1. Headers on row 1 — column names are case-sensitive
2. Pipe (`|`) for lists — e.g. `Ferrite|Kool Mu` for alternatives (kept as a raw string through the API, not parsed)
3. No extra spaces around values
4. Numbers without units — store `250` not `250µH`
5. UTF-8 encoding

---

## Where Data Currently Comes From
- **cores.csv:** Magnetics Inc. datasheets (all 16 current rows are Magnetics-brand parts)
- **materials.csv:** material vendor datasheets
- **reference_designs.csv:** one real IntelliPower part (`i77006`), used for manual validation
- **test_scenarios.csv:** manually authored scenarios, including the `i77006` reference case