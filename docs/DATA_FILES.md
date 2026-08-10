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
**Location:** `data/real_materials.csv`, `data/real_cores.csv`,
`data/real_core_loss_coefficients.csv`
**Used by:** `python/services/magnetics_data.py`
**Format:**
`PartNumber,Material,Mu,AL,Ae,Wa,Le,Mlt,PartCost,Vendor,MaxCurrent_A,MaxFreq_kHz,CoreShape,ShapeFamily,MaterialType,DatasheetUrl,WindowWidthMm,WindowHeightMm,SurfaceAreaWoundMm2`
for cores, `Name,MuOpt,MinFrequencyHz,MaxFrequencyHz,Reason,Alternatives,BmaxT,MaterialType,Manufacturer,DatasheetUrl`
for materials, and `MaterialName,MinFrequencyHz,MaxFrequencyHz,K,Alpha,Beta,Ct0,Ct1,Ct2`
(one row per frequency range) for the Steinmetz core-loss coefficients.
**Currently:** 34 materials, 755 cores — real data, all Magnetics Inc.,
transcribed by hand from mag-inc.com's own live catalog (Advanced Part
Number Finder), not sampled from any third-party library. Scope is
deliberately narrowed to two Magnetics powder families: MPP toroids (432
parts, all 12 real permeability grades, 14μ–550μ) and Magnetics E-cores
(323 parts across the Kool Mu/Edge/XFlux/High Flux material families).
Every other family a much earlier snapshot of this project carried
(ferrite, other powder toroid families, non-E-core shapes) was removed as
out of scope. `CoreShape` is always `Toroid` or `TwoPieceSet` and
`ShapeFamily` is always `T` or `E` (see "Core shape is now real" below).
`MaterialType` is always `powder` (see "Ferrite toroids vs. powder
toroids" below for why that field, not shape alone, is what the gap-design
formula actually branches on). `DatasheetUrl`/`Manufacturer`/`WindowWidthMm`/
`WindowHeightMm` are covered in "Columns added after a database audit"
below.

**Why three separate files, not one:** materials:cores is a real
many-to-one relationship (755 cores share 34 materials) - merging them
would repeat every material-level fact (MuOpt, BmaxT, Reason, ...) on
every core row that uses it. materials:core-loss-coefficients is a real
one-to-many relationship with a variable row count per material (0 to
several frequency ranges, some have zero, some have multiple) - that
can't become flat columns on a single row without either an arbitrary cap
or a non-tabular nested structure. Three normalized files (four, counting
`dc_bias_curves.csv` below), joined in memory at evaluation time by the
`Material`/`MaterialName` string key (see `MaterialEvaluation.cpp`/
`CoreEvaluation.cpp`), is the correct relational shape for this data, not
a workaround.

**These files are hand-curated, not generated.** There is no script that
regenerates them - the old PyOpenMagnetics-sampling export script this
project used to have (`scripts/export_real_data.py`) has been deleted. To
change what's in them, edit the CSVs directly, sourcing any new/changed
values from Magnetics Inc.'s own live catalog (mag-inc.com) by hand, the
same way this snapshot was built - see `docs/ARCHITECTURE.md` → "Data
Source" for the full process.

**Known Phase 1 data gaps (real limitations, not bugs in the engine):**
- `real_materials.csv`'s `BmaxT` is real, material-specific saturation flux
  density data (transcribed by hand from Magnetics' own material curve
  pages) for all 34 materials — `PeakFluxValidation`/`SaturationValidation`
  use it automatically instead of `DesignRules.defaultFluxDensityLimitT`
  (`usesDefaultAssumption: false` when it's in use).
- `real_core_loss_coefficients.csv` now has real Steinmetz (`k`/`alpha`/
  `beta`) coefficients for all 34 materials — Magnetics does publish them
  for MPP/Kool Mu-family materials, just not in a bulk per-part table like
  AL/Wa; each material has its own "Core Loss Density Curves" fit-formula
  page (`P[mW/cm³] = a·Bᵇ·fᶜ`, transcribed by hand and unit-converted to
  this engine's `Pv[W/m³] = k·f[Hz]^alpha·B^beta` form). Core loss now
  reports a real watt value (`Evaluated`) for any candidate whenever
  `rippleCurrentPeakToPeakA` is supplied, instead of always `NotEvaluated`.
- `real_cores.csv`'s `Mlt` column is real for all 755 cores — MPP toroids
  derived by hand from each part's real OD/ID/HT, E-cores derived from each
  unique physical size's real center-leg width/depth dimensions (see the
  `WindowWidthMm`/`WindowHeightMm` note below for how those were identified).
  Fill factor and current density never needed it; total wire length and DCR
  now use it for every core (`winding.resistanceStatus: Evaluated`) instead
  of only MPP toroids.
- `real_cores.csv`'s `PartCost` and `MaxCurrent_A` columns are still 0.0 for
  every row — not currently used by any Phase 1 check.
- **Core shape** (`CoreShape`/`ShapeFamily` columns) is always `Toroid`/`T`
  (MPP) or `TwoPieceSet`/`E` (E-cores) — assigned by hand from each part's
  real mag-inc.com shape category, since scope is narrowed to just those
  two Magnetics powder shapes. Surfaced in the API
  (`core.coreShape`/`core.shapeFamily`) and the frontend (a shape badge per
  candidate plus a shape filter dropdown).
- **`MaterialType` column is always `powder`** in this snapshot. There is
  no ferrite in the current database at all, so
  `TurnsAndGapDesign.cpp`'s `CoreShape == "Toroid" AND MaterialType ==
  "powder"` distributed-gap branch is the only gap-design path any current
  candidate can take — the real machined-gap formula (`GapDesign.cpp`)
  still exists in the engine and is still exercised by tests (with a
  synthetic ferrite fixture, since no real ferrite part remains), but has
  no live database row to run against today.
- `DatasheetUrl` (materials and cores) — real Magnetics datasheet PDF
  links (`mag-inc.com/Media/Magnetics/Datasheets/<PartNumber>.pdf`),
  populated for every row. `Manufacturer`/`Vendor` are always `Magnetics`.
- `WindowWidthMm`/`WindowHeightMm` (cores) — real for all 323 E-core rows,
  pulled from each unique physical size's real Dimensions table on the
  datasheet's isometric drawing (letters "D"=height, "E"=width, identified
  by tolerance type and confirmed by internal consistency - see
  `magnetics_data.py`'s module docstring for the full reasoning). Blank for
  MPP toroids, which never have a flat width/height by definition (their
  window is described by `Wa`/`ID` instead). `WindingDesign.cpp`'s
  parallel-strand bundle-fit check (`bundleFitStatus`) now evaluates for
  real E-core candidates instead of always `NotEvaluated`.
- **Deliberately not added:** distributor cost (`PartCost` stays `0.0` for
  every row — no check in this project consumes cost today).
- `SurfaceAreaWoundMm2` (cores) — the real, manufacturer-published external
  surface area of the wound coil, transcribed from each datasheet's own
  "Surface Area" table (the "N% Winding Factor" row) when that table exists
  for that part's layout. Populated for 423 of 755 cores — present on MPP
  toroid datasheets, absent from every E-core datasheet layout (no such
  table exists there, confirmed by inspection, not a transcription gap).
  Blank (`0.0`) means not yet transcribed or not published for that layout
  — never guessed or back-filled from a formula. Feeds
  `ThermalEvaluation.cpp`'s `estimateThermalResistanceCPerW()` as the
  highest-priority tier of its Rth estimate, ahead of the Ae×Le
  shape-factor estimate that runs when this is `0.0`.

---

## dc_bias_curves.csv

**Purpose:** Real, manufacturer-published DC-bias permeability roll-off curve-fit coefficients for powder
(distributed-gap) toroid materials - lets `TurnsAndGapDesign.cpp` compute a real, current-dependent
effective AL instead of the flat 0-bias catalog AL. See `docs/ARCHITECTURE.md` → "Powder-Core DC-Bias
Permeability Roll-Off" for the full mechanism.
**Location:** `data/dc_bias_curves.csv`
**Used by:** `python/services/magnetics_data.py`'s `fetch_dc_bias_curves()` → `DCBiasCurveDatabase`
(`src/data/DCBiasCurveDatabase.h`)
**Format:** `MaterialName,A,B,C,D,Vendor,DatasheetUrl` - `%initial_permeability = 1/(A + B*H^C) + D`, H in
Oersteds. `MaterialName` matches `real_materials.csv`'s `Name` column exactly (e.g. `MPP 60`).
**Currently:** 33 rows, one per material in the current MPP/E-core-only
database (`real_materials.csv`) that has a published DC-bias curve. Every
row has `Vendor=Magnetics` and a real `mag-inc.com` material-curves URL -
these are Magnetics' own published "Permeability vs. DC Bias" coefficients.
The `A`/`B`/`C`/`D` fit values themselves were originally computed via
PyOpenMagnetics' digitization of those same published Magnetics curves
(not independently invented data) before this project stopped depending on
PyOpenMagnetics for anything else; a handful (MPP 60, Kool Mµ 60, High Flux
26, XFlux 60, Edge 60) were cross-checked by hand against the vendor's own
published "Permeability vs. DC Bias" page/datasheet PDF and matched to 4+
significant figures - see `tests/cpp/PermeabilityRolloffTests.cpp`. 100 rows
for materials outside the current database's scope (ferrite and other
powder families this project no longer carries) were pruned from the
original 133-row set.

**Known Phase 1 data gap:** not every material in `real_materials.csv` has a
published DC-bias curve - materials with none simply have no row here. Any
such distributed-gap material falls back to the pre-existing flat-AL0
behavior - `TurnsAndGapResult::usesDCBiasRolloffCurve` stays `false` so
callers can tell the difference, never a silently-assumed 0%-bias result.

**Do not hand-edit the coefficients.** They're read verbatim from the manufacturer's own published tables;
if a value looks wrong, re-check it against the datasheet URL in that row rather than adjusting it here.

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
- `Ae` and `Wa` are in mm² (not cm²) — this is what `src/core/sizing/CoreEvaluation.cpp` (formerly `CoreSelection.cpp`, since removed) expects; it converts internally via `(Ae × Wa) × 1e-4` to get cm⁴
5. This exact file (`cores.csv`) is gone and isn't read by the app. If you meant `data/real_cores.csv` (the current file): editing it directly *is* how you change this data — it's hand-curated, not generated, so there's no script to re-run. Just make sure any new/changed values are sourced from Magnetics Inc.'s own live catalog (mag-inc.com), the same way the rest of the snapshot was built.

### Calculation Tips

`src/core/sizing/CoreEvaluation.cpp`'s core-side area product: `Ap_cm4 = (Ae_mm² × Wa_mm²) × 1e-4`. This is a different Ap than the one computed by `src/core/sizing/AreaProduct.cpp` (which computes the *required* Ap from L, Ipk, Ku, Bmax, J) — the two are compared against each other in Stage 3, not the same calculation.

If a datasheet gives µ instead of AL:
```
AL ≈ 0.4π × µ₀ × µᵣ × (Ae / Le) × 10⁹ (nH/100T; µ₀ = 4π×10⁻⁷, Ae in cm², Le in cm — convert your mm values before using this formula)
```

---

## materials.csv

**Purpose:** Database of magnetic materials and their properties.
**Location:** `data/materials.csv`
**Used by:** formerly `src/data/Materials.cpp` (now deleted, same reason as `CoreDatabase.cpp` above); `src/core/sizing/MaterialEvaluation.cpp` (formerly `MaterialSelection.cpp`, since removed) does the real work today via `src/data/MaterialDatabase.h`
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
| BmaxT | T | 1.0 | Max flux density — as of today's Phase 1 engine, `PeakFluxValidation`/`SaturationValidation` (`DesignValidation.cpp`) prefer a material's own `BmaxT` over the `DesignRules` default (0.30 T) whenever it's populated. (This is the deprecated hand-typed format — the real snapshot, `real_materials.csv`, has real `BmaxT` for all 34 materials in the current snapshot; see the section above.) Not a hard-coded value in `src/core/sizing/AreaProduct.cpp` anymore - see [FORMULAS.md](FORMULAS.md) section 7 |
| CuLossFactor | — | 1.15 | Multiplier for AC copper loss — retired from the real snapshot in favor of real Steinmetz coefficients in `data/real_core_loss_coefficients.csv` (see the section above), now driving a real core-loss computation whenever ripple current is supplied. See [FORMULAS.md](FORMULAS.md) section 9 |

### Frequency Ranges (current data)
- **Powder Iron:** 1–100 kHz — reason text notes "good for buck inductors" specifically; worth genericizing if the tool is meant to be topology-agnostic
- **Kool Mu:** 50–250 kHz
- **Ferrite 3C90:** 50–250 kHz
- **High Frequency Ferrite:** 250 kHz–10 MHz

### How to Add a New Material

1. Get the datasheet (µ, Bmax, loss curves)
2. Open `data/materials.csv`
3. Add a new row matching the 8-field order above
4. Check frequency range overlaps deliberately — `src/core/sizing/MaterialEvaluation.cpp` now returns every material whose range contains the request as its own candidate, not just the first match in file order (that was the old `MaterialSelection.cpp` behavior, since removed)
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
Add more real designs here as they become available — each new row is another point to validate the tool's core selection and turns/gap design against (`tests/python/test_reference_designs.py`). Note that `real_cores.csv` is currently Magnetics-only (MPP toroids and Kool Mu/Edge/XFlux/High Flux E-cores), so a reference part sourced from a different manufacturer's catalog, like `i77006`, won't have a matching core part number in the live database — `test_expected_core_and_turns_match_original_catalog` is `xfail`-marked for this exact reason, not a bug.

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
Run `pytest tests/python` from the repo root. `test_scenario_produces_a_feasible_or_honestly_infeasible_result` runs every row through `magnetics_cpp.run_inductor_design()` and checks the engine never crashes and always returns a well-formed status. `test_expected_core_and_turns_match_original_catalog` is `xfail`-marked, not skipped: `ExpectedCore`/`ExpectedTurns` values (e.g. `0077440A7`) were calibrated against a catalog whose part numbers don't exist in `data/real_cores.csv` (Magnetics-only, MPP toroids and Kool Mu/Edge/XFlux/High Flux E-cores) — a real data-source mismatch between this fixture and the live core database, not an engine bug.

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
- **Cores and materials:** `data/real_materials.csv` / `data/real_cores.csv` — real Magnetics Inc. data, transcribed by hand from mag-inc.com's own live catalog, read as plain CSV at startup. No third-party magnetics library involved. See `docs/ARCHITECTURE.md`.
- **reference_designs.csv:** one real IntelliPower part (`i77006`), used for manual validation
- **test_scenarios.csv:** manually authored scenarios, including the `i77006` reference case