# Data Files Guide

> **⚠️ `cores.csv` and `materials.csv` (the original hand-typed files) no
> longer exist.** They've been replaced by **`data/real_materials.csv`**
> and **`data/real_cores.csv`** — same idea (CSV, read at startup), but the
> *contents* are real, sourced data instead of hand-typed values. See
> "real_materials.csv / real_cores.csv" below for the current format, and
> `docs/ARCHITECTURE.md` → "Data Source" for why they're a bundled
> snapshot rather than a live query. The sections further down describing
> the *old* `cores.csv`/`materials.csv` format are kept only as historical
> reference.
>
> `reference_designs.csv` and `test_scenarios.csv` are **unaffected** —
> they're the validation suite, not a data source, and are still read
> directly, same as always. Note: `test_scenarios.csv`'s `ExpectedCore`
> values (e.g. `0077440A7`) are old part numbers from the original CSVs —
> real data uses different part numbers, so those expected values need to
> be re-verified and updated, not treated as still valid.

---

## real_materials.csv / real_cores.csv (current, actually used)

**Purpose:** The real material/core database, read at FastAPI startup.
**Location:** `data/real_materials.csv`, `data/real_cores.csv`
**Used by:** `python/services/magnetics_data.py`
**Format:** Same columns as the old `cores.csv`/`materials.csv` described
below — `PartNumber,Material,Mu,AL,Ae,Wa,Le,PartCost,Vendor,MaxCurrent_A,MaxFreq_kHz`
and `Name,MuOpt,MinFrequencyHz,MaxFrequencyHz,Reason,Alternatives,BmaxT,CuLossFactor`
respectively.
**Currently:** 32 materials, 60 cores — real data (Ferroxcube, TDK,
Magnetics, Fair-Rite), filtered to power-application materials and
ungapped cores, spread across vendors.
**Do not hand-edit these files.** To change what's in them, either adjust
the filters in `scripts/export_real_data.py` and re-run it (needs
PyOpenMagnetics installed — Linux/macOS/WSL2 only, see
`docs/ARCHITECTURE.md`), or send them to someone/somewhere that can run
that script and swap the resulting files in.

**Known Phase 1 data gaps (real limitations, not bugs in the engine):**
- `real_materials.csv`'s `BmaxT` and `CuLossFactor` columns exist but are
  **0.0 for every one of the 32 materials** — `scripts/export_real_data.py`
  never populated them from the source data. Effects: `PeakFluxValidation`/
  `SaturationValidation` always fall back to `DesignRules.defaultFluxDensityLimitT`
  (flagged `usedDefaultLimit: true`, never presented as a material fact),
  and `CoreLoss` is never invoked with real coefficients (`losses.coreLossStatus`
  is always `NotEvaluated`).
- `real_cores.csv` has **no mean-length-per-turn (MLT) or bobbin/winding-height
  column** — only `Ae`, `Wa`, `Le`, `AL`, `Mu`. Fill factor and current
  density are still fully computed (they only need turns × wire area ÷ Wa),
  but total wire length and DCR are always `NotEvaluated`
  (`winding.resistanceStatus`), which also blocks DC copper loss
  (`losses.copperLossStatus`).
- `real_cores.csv`'s `PartCost` and `MaxCurrent_A` columns are also 0.0 for
  every row — not currently used by any Phase 1 check.

Closing these gaps means re-running `scripts/export_real_data.py` with those
fields actually populated, or adding real per-part datasheet values by
hand — neither is done in Phase 1; the engine reports the gap honestly
(`not_evaluated` + a `missingData` explanation) instead of guessing.

---

This document also explains the structure the *original* CSV files used
to have — useful background if you're editing `python/services/magnetics_data.py`'s
field mapping, since that's what these columns became.

---

## cores.csv (historical schema reference only — file no longer exists)

**Purpose:** Database of available inductor cores (part numbers, geometry, materials).
**Location:** `data/cores.csv`
**Used by:** formerly `src/data/CoreDatabase.cpp` (now deleted - it was fully dead code, `DATA_SOURCES` was always empty in `CMakeLists.txt`); real data is populated by `python/services/magnetics_data.py` via `CoreDatabase.h`'s `setData()`/`load()` instead
**Currently:** mapped from real data at startup, not from this file

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
- `Ae` and `Wa` are in mm² (not cm²) — this is what `CoreEvaluation.cpp` (formerly `CoreSelection.cpp`, since removed) expects; it converts internally via `(Ae × Wa) × 1e-4` to get cm⁴
5. This exact file (`cores.csv`) is gone and isn't read by the app. If you meant `data/real_cores.csv` (the current file): editing it directly *would* take effect on restart, but don't — it's a generated snapshot. Adjust the filters in `scripts/export_real_data.py` and re-run it instead, so the file stays reproducible.

### Calculation Tips

`CoreEvaluation.cpp`'s core-side area product: `Ap_cm4 = (Ae_mm² × Wa_mm²) × 1e-4`. This is a different Ap than the one computed by `AreaProduct.cpp` (which computes the *required* Ap from L, Ipk, Ku, Bmax, J) — the two are compared against each other in Stage 3, not the same calculation.

If a datasheet gives µ instead of AL:
```
AL ≈ 0.4π × µ₀ × µᵣ × (Ae / Le) × 10⁹ (nH/100T; µ₀ = 4π×10⁻⁷, Ae in cm², Le in cm — convert your mm values before using this formula)
```

---

## materials.csv

**Purpose:** Database of magnetic materials and their properties.
**Location:** `data/materials.csv`
**Used by:** formerly `src/data/Materials.cpp` (now deleted, same reason as `CoreDatabase.cpp` above); `MaterialEvaluation.cpp` (formerly `MaterialSelection.cpp`, since removed) does the real work today via `Materials.h`
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
| BmaxT | T | 1.0 | Max flux density — as of today's Phase 1 engine, `PeakFluxValidation`/`SaturationValidation` (`DesignValidation.cpp`) prefer a material's own `BmaxT` over the `DesignRules` default (0.30 T) whenever it's populated - currently none are, in the real data snapshot, so the default is what's actually used. Not a hard-coded value in `AreaProduct.cpp` anymore - see [FORMULAS.md](FORMULAS.md) section 7 |
| CuLossFactor | — | 1.15 | Multiplier for AC copper loss — `CoreLoss.cpp` is implemented and would use it, but is gated on this being populated, which it isn't in the real data snapshot; see [FORMULAS.md](FORMULAS.md) section 9 |

### Frequency Ranges (current data)
- **Powder Iron:** 1–100 kHz — reason text notes "good for buck inductors" specifically; worth genericizing if the tool is meant to be topology-agnostic
- **Kool Mu:** 50–250 kHz
- **Ferrite 3C90:** 50–250 kHz
- **High Frequency Ferrite:** 250 kHz–10 MHz

### How to Add a New Material

1. Get the datasheet (µ, Bmax, loss curves)
2. Open `data/materials.csv`
3. Add a new row matching the 8-field order above
4. Check frequency range overlaps deliberately — `MaterialEvaluation.cpp` now returns every material whose range contains the request as its own candidate, not just the first match in file order (that was the old `MaterialSelection.cpp` behavior, since removed)
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
Add more real designs here as they become available — each new row is another point to validate the tool's core selection and turns/gap design against (`tests/python/test_reference_designs.py`). Note that `real_cores.csv` is currently Ferroxcube-only, so a Kool Mu/Powder Iron reference part like `i77006` won't have a matching core in the live database yet — see the data gap note above.

---

## test_scenarios.csv

**Purpose:** Test cases for validating the design algorithm, checked automatically by `pytest tests/python/test_reference_designs.py`.
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
| ExpectedTurns | 14-20 | Expected turns range — checked automatically against `run_inductor_design()`'s output |
| TestDescription | "Real IntelliPower spec; should match" | Why this scenario matters |

### How to Run Tests (current process)
Run `pytest tests/python` from the repo root. `test_scenario_produces_a_feasible_or_honestly_infeasible_result` runs every row through `magnetics_cpp.run_inductor_design()` and checks the engine never crashes and always returns a well-formed status. `test_expected_core_and_turns_match_original_catalog` is `xfail`-marked, not skipped: `ExpectedCore`/`ExpectedTurns` values (e.g. `0077440A7`) were calibrated against a Kool Mu/Powder Iron catalog that doesn't exist in `data/real_cores.csv` (Ferroxcube-only) — a real data-source mismatch between this fixture and the live core database, not an engine bug.

### Adding Test Cases
When you find a bug or edge case:
1. Add a row with the failing scenario
2. Fix the code
3. Re-run `pytest tests/python` to confirm
4. Keep the row for future regression checking

---

## CSV Format Rules
1. Headers on row 1 — column names are case-sensitive
2. Pipe (`|`) for lists — e.g. `Ferrite|Kool Mu` for alternatives (kept as a raw string through the API, not parsed)
3. No extra spaces around values
4. Numbers without units — store `250` not `250µH`
5. UTF-8 encoding

---

## Where Data Currently Comes From
- **Cores and materials:** `data/real_materials.csv` / `data/real_cores.csv` — real manufacturer data (Ferroxcube, TDK, Magnetics, Fair-Rite, and others) originally sourced from PyOpenMagnetics/MAS, exported once via `scripts/export_real_data.py`, read as plain CSV at startup. See `docs/ARCHITECTURE.md`.
- **reference_designs.csv:** one real IntelliPower part (`i77006`), used for manual validation
- **test_scenarios.csv:** manually authored scenarios, including the `i77006` reference case