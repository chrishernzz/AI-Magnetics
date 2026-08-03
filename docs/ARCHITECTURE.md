# System Architecture

This document describes the overall design of AIMagnetics: components, responsibilities, and data flow.

---

## High-Level Overview

AIMagnetics is a **four-layer system**:

1. **Frontend (Web UI)** — plain HTML/JS/CSS; user enters requirements, displays results
2. **Backend (FastAPI, Python)** — routes HTTP requests to the C++ engine via pybind11, returns JSON
3. **Python Bindings (pybind11)** — exposes the C++ engine's classes/functions to Python as the `magnetics_cpp` module
4. **Core Engine (C++17 library)** — implements the magnetic design algorithms

There is no hand-written HTTP server — FastAPI (via `uvicorn`) handles all HTTP.

---

## Component Breakdown

### Frontend Layer

**Location:** `frontend/`

| File | Purpose |
|---|---|
| `index.html` | Web page structure; form for requirements input |
| `app.js` | Handles form submission, calls the API, renders results |
| `styles.css` | Styling for the UI |

**Endpoint it calls** (relative path, no `/api` prefix):
- `POST /inductor-design` — the single, canonical Phase 1 endpoint; runs the whole pipeline once per request

Payload shape: `InductorDesignRequest` — `{ inductanceUH, peakCurrentA, rmsCurrentA, switchingFreqKHz, ambientTemperatureC, allowableTempRiseC, inductanceTolerancePercent, ... }` (see [API_REFERENCE.md](API_REFERENCE.md)).

The four old single-stage endpoints (`/material-selection`, `/calculate`, `/core-selection`, `/turns-calculation`) have been removed - once the frontend stopped calling them, they were deleted outright along with the single-pick C++ they were backed by, rather than left as permanent dead weight.

---

### Backend Layer — FastAPI (Python)

**Location:** `python/`

| File | Purpose |
|---|---|
| `app.py` | Creates the FastAPI app, mounts `/static` for the frontend, serves `index.html` at `/`, includes both routers |
| `routes/inductor_design.py` | Defines `InductorDesignRequest` (the renamed, extended successor to the old shared `BuckInput`) and the `POST /inductor-design` endpoint (Mode 2 - direct entry); also owns the recursive C++-struct-to-dict serializer used to turn a `DesignRecommendation` into JSON |
| `routes/topology_design.py` | Defines `BuckTopologyInput` and the `POST /topology-design/buck` endpoint (Mode 1 - converter-level entry); serializes the derived `InductorDesignRequest` back to JSON, ready to feed straight into `/inductor-design` |

`routes/core_selection.py` (the old deprecated single-stage endpoints) was deleted once nothing called it anymore.

**Routes defined:**
| Route | Calls into C++ | Returns |
|---|---|---|
| `POST /inductor-design` | `InductorDesignService::run(...)` (via `magnetics_cpp.run_inductor_design`) — the full pipeline, once | `DesignRecommendation` (serialized to a plain dict) |
| `POST /topology-design/buck` | `BuckElectricalSolver::solve(...)` (via `magnetics_cpp.solve_buck_topology`) — derives requirements only, does not run the pipeline | `InductorDesignRequest`-shaped dict, ready for `/inductor-design` |
| `GET /` | — | `index.html` (via `FileResponse`) |
| `GET /static/*` | — | frontend static files (mounted via `StaticFiles`) |

Request/response validation is handled by a Pydantic model declared directly in the route file — there is no separate "Controller" class; the route function body does the parsing, calling, and response-shaping in one place. The route hard-codes no magnetic-design constant — `Ku`/`Bmax`/`J` are always sourced from `magnetics_cpp.design_rules_phase1_default()` (bound from `DesignRules::phase1Default()` in C++), never hard-coded in Python (spec section 7).

---

### Python Bindings Layer — pybind11

**Location:** `src/python_bindings/InductorDesignBindings.cpp` (renamed from `CoreSelectionBindings.cpp`)

Builds a single pybind11 module, `magnetics_cpp`, exposing:
- `AreaProductInput` + `calculate_ap()`/`calculate_stored_energy()` — used internally by the Phase 1 pipeline and directly by `tests/python/test_unit_conversions.py`
- Raw data: `CoreData`, `MaterialData` (carrying `bmaxT`/`cuLossFactor`) + `set_core_database()`/`set_material_database()`
- Phase 1 engine: `EvaluationStatus` (enum), `DesignRules` + `design_rules_phase1_default()`, `MaterialCandidate`, `CoreCandidate`, `TurnsAndGapResult`, `ValidationResult`, `WindingDesignResult`, `LossEvaluationResult`, `ThermalEvaluationResult`, `RejectionReason`, `InductorCandidate`, `DesignRecommendation`, `InductorDesignRequest`, and the entry point `run_inductor_design()`
- Mode 1 (Buck): `TopologyInput` (its `topology` field is not exposed to Python - the struct's default constructor already sets `Topology::Buck`, the only implemented value) and the entry point `solve_buck_topology()`

The old single-pick bindings (`MaterialSelectionInput`/`Result`/`Service`,
`CoreSelectionInput`/`Result`/`Service`, `TurnsCalculationInput`/`Result` +
`calculate_turns()`, `select_core()`) were removed along with the
deprecated endpoints they existed only to serve.

CMake builds this as a Python extension module and places the compiled `.pyd`/`.so` directly in `python/`, so `import magnetics_cpp` resolves it as a local module.

---

### Core Engine Layer

**Location:** `src/core/`, `src/backend/services/`, `src/data/`

| Module | File | Status |
|---|---|---|
| AreaProduct | `src/core/sizing/AreaProduct.cpp` | ✅ Implemented — energy/Ap sizing, called directly by `InductorDesignService` |
| TurnsCalculation | `src/core/magnetics/TurnsCalculation.cpp` | ✅ Implemented — `N = round(sqrt(L_nH/AL_nH))`, verified against the real i77006 reference design. This was previously documented (in most other files) as an unimplemented stub — it was not; only the docs were wrong. Reused as the seed-turns estimator inside `src/core/magnetics/TurnsAndGapDesign.cpp` |
| GapDesign | `src/core/magnetics/GapDesign.cpp` | ✅ Implemented — series-reluctance gapped-core AL formula, verified numerically against `data/real_cores.csv` to <0.03% |
| TurnsAndGapDesign | `src/core/magnetics/TurnsAndGapDesign.cpp` | ✅ Implemented — iterates turns and gap together until the integer turns count stabilizes or is rejected (impractical gap, non-convergence) |
| MaterialEvaluation | `src/core/sizing/MaterialEvaluation.cpp` | ✅ Implemented — `findSuitableMaterials()`, returns every frequency-compatible material as its own candidate |
| CoreEvaluation | `src/core/sizing/CoreEvaluation.cpp` | ✅ Implemented — `findSuitableCores()`, returns every material-compatible core with its own `meetsAreaProduct` flag; never silently substitutes an oversized core |
| DesignValidation | `validation/DesignValidation.cpp` | ✅ Implemented — six named checks (Inductance, PeakFlux, Saturation, WindingFit, CurrentDensity, Thermal), each its own `ValidationResult` (now also carrying `isPreliminaryEstimate`); plus `calculateFluxLimitTiers()`, an informational 2-of-4-real flux-limit breakdown |
| WindingDesign | `src/core/winding/WindingDesign.cpp` | ✅ Implemented — AWG wire selection, raw copper fill factor, current density always computed; a separate realistic **physical window fill** (insulation, packing factor, bobbin/margin/lead-exit derates) is what `WindingFitValidation` actually gates on. DCR (now including lead/routing/connection resistance) and wire length real for most cores via a geometry-derived mean-length-per-turn estimate, `not_evaluated` only for cores whose upstream geometry doesn't support it |
| CopperLoss | `src/core/losses/CopperLoss.cpp` | ✅ Implemented — `Pcu_dc = Irms^2 * DCR`, only called when DCR is available; the reported figure is overwritten with the thermal loop's converged hot-DCR value when that loop converges |
| CoreLoss | `src/core/losses/CoreLoss.cpp` | ✅ Implemented — real Steinmetz equation `Pv = k*f^alpha*B^beta` (W/m³) using `data/real_core_loss_coefficients.csv`; `Evaluated` when the material has coefficients at this frequency, the request supplies `rippleCurrentPeakToPeakA` (needed for flux-density swing — never approximated from peak flux), AND the computed swing falls within the coefficient's optional valid range (currently always unset, so a documented no-op), `NotEvaluated` otherwise. Temperature correction (ct0/ct1/ct2) not yet applied |
| SkinDepthRisk | `src/core/losses/SkinDepthRisk.cpp` (renamed from the dead `HighFrequencyLosses.cpp` stub) | ✅ Implemented — real qualitative Low/Moderate/High AC-loss risk from strand-radius-vs-skin-depth; `acLossWattsStatus` permanently `NotEvaluated` (no AC-loss watts model exists — a risk level is not a watts figure) |
| LossEvaluation | `src/core/losses/LossEvaluation.cpp` | ✅ Implemented — orchestrates CopperLoss/CoreLoss (SkinDepthRisk is evaluated separately by the caller, not part of this struct), reports each as `Evaluated`/`NotEvaluated`, plus core-loss detail fields (material used, coefficient frequency range, flux swing, volume, density) |
| ThermalEvaluation | `src/core/thermal/ThermalEvaluation.cpp` | ✅ Implemented — real iterative convergence loop (temp → hot DCR → copper loss → temp rise → repeat) using a size-aware Rth estimate derived from each candidate's own real Ae/Le geometry via Newton's law of cooling (`naturalConvectionCoefficientWPerM2K`/`compactSolidSurfaceAreaShapeFactor`), falling back to the flat `DesignRules.defaultThermalResistanceCPerW` only when that geometry is unavailable — neither is per-core measured data. Caps at `PreliminaryThermalEstimate` — `ThermalStatus` has no "fully evaluated" value. `NotEvaluated` when DCR geometry is unknown or the loop's real positive-feedback iteration diverges |
| RecommendationStatus | `src/validation/RecommendationStatus.cpp` | ✅ Implemented — 3-tier `Pass`/`ConditionalPass`/`Reject` classification replacing the old frontend-only "Recommended" sugar; `Pass` currently unreachable in practice (ThermalValidation always flags isPreliminaryEstimate) |
| DataCache<T> | `src/data/DataCache.h` | ✅ Implemented — shared template holding the "cache once, warn if empty" logic that `CoreDatabase`, `MaterialDatabase`, and `CoreLossCoefficientDatabase` all use, instead of each repeating it |
| CoreDatabase | `src/data/CoreDatabase.h` | ✅ Implemented — real data loaded once at startup from a bundled snapshot (`data/real_cores.csv`, hand-curated from Magnetics Inc.'s own catalog — see "Data Source" below); no CSV fallback, startup fails loudly if this fails. `load()` returns by `const&`, not by value |
| MaterialDatabase | `src/data/MaterialDatabase.h` | ✅ Implemented — same pattern as CoreDatabase; `MaterialData` now also carries a real, material-specific `bmaxT` for all 165 materials |
| Validation | `src/validation/Validation.h` | ✅ Implemented — `ValidationResult{passed, checkName, calculatedValue, limitValue, unit, explanation, usesDefaultAssumption}`, compiled via `VALIDATION_SOURCES` |
| DesignRules | `src/rules/DesignRules.h`/`.cpp` | ✅ Implemented — `DesignRules::phase1Default()` is the single source of Ku/Bmax/J/tolerance defaults, compiled via `RULES_SOURCES` |
| RequirementDerivationService | `backend/services/RequirementDerivationService.cpp` | ✅ Implemented — unit conversion + RMS-current-from-ripple derivation (triangular ripple only) |
| InductorDesignService | `backend/services/InductorDesignService.cpp` | ✅ Implemented — the single orchestrator behind `POST /inductor-design` |
| BuckElectricalSolver | `backend/services/BuckElectricalSolver.cpp` | ✅ Implemented — Mode 1, derives an `InductorDesignRequest` (inductance, peak current, average current + ripple) from Buck converter requirements, sized at the worst-case Vin_max. Does not compute RMS current itself - leaves that to `RequirementDerivationService` downstream, so there is exactly one RMS derivation in the codebase regardless of which mode produced the request |

The **Services** layer (`src/backend/services/`) sits directly between the pybind11 bindings and the core engine — e.g. `InductorDesignService::run()` orchestrates `MaterialEvaluation` → `AreaProduct` → `CoreEvaluation` → `TurnsAndGapDesign` → `DesignValidation` → `WindingDesign` → `LossEvaluation` → `ThermalEvaluation`. There is no separate "Controller" layer in C++; HTTP parsing happens entirely in the Python route functions. `BuckElectricalSolver` sits beside `InductorDesignService`, not inside it - it produces an `InductorDesignRequest` and stops, rather than running the pipeline itself, so Mode 1 is strictly "one more way to produce Mode 2's input" and not a second, parallel pipeline.

---

## Data Flow: From User Input to Result

**Mode 1 (optional, Buck converter entry):**
```
User enters: Vin=36-60V, Vout=12V, Iout=40A, f=500kHz, ripple=20% of Iout
↓
Frontend POSTs to /topology-design/buck with the BuckTopologyInput payload
↓
FastAPI route (routes/topology_design.py) builds a C++ TopologyInput,
calls magnetics_cpp.solve_buck_topology(...)
↓
BuckElectricalSolver::solve() (C++): sizes L and ripple current at the
worst-case Vin_max, computes Ipeak; leaves rmsCurrentA unset
↓
Route serializes the derived InductorDesignRequest-shaped fields to JSON
↓
Frontend shows a "Derived from your Buck converter requirements" banner,
pre-fills the Mode 2 fields below with it, and the flow continues exactly
as Mode 2's does from here - this call never touches InductorDesignService
or runs the pipeline itself.
```

**Mode 2 (direct entry, or continuing from Mode 1's derived values):**
```
User enters: L=250µH, Ipk=5A, Irms=3.5A, f=100kHz, Tambient=25°C, ΔT=40°C
↓
Frontend POSTs to /inductor-design with the InductorDesignRequest payload
↓
FastAPI route (routes/inductor_design.py) builds a C++ InductorDesignRequest,
calls magnetics_cpp.run_inductor_design(...)
↓
InductorDesignService::run() (C++):
  1. RequirementDerivationService::derive() - unit conversion, RMS-current
     derivation if needed (never inferred from peak current alone)
  2. findSuitableMaterials() - every frequency-compatible material, not just
     the first match
  3. calculateAp() using DesignRules::phase1Default() (no hidden constants)
  4. findSuitableCores() - every material-compatible core, each flagged
     meetsAreaProduct; if none pass -> return status "no_feasible_design"
     with requiredAreaProductCm4 / largestAvailableAreaProductCm4, not a
     silent oversized substitute
  5. For each feasible core: designTurnsAndGap() (iterates turns and gap
     together - when peakCurrentA is known, the starting turns count is
     raised, never lowered, above the plain inductance-matching minimum
     whenever needed to respect the material's saturation margin, so a real
     gapped design isn't missed just because the naive minimum-turns point
     saturates - see TurnsAndGapResult::turnsRaisedForSaturationMargin),
     then DesignValidation's checks (including PeakFluxValidation/
     SaturationValidation, which still independently verify the result),
     designWinding(), evaluateLosses(), evaluateThermal()
  6. Passing candidates ranked by real total loss (copper + core, whichever
     are Evaluated) ascending - the actual "Optimization" half of Option 2
     (Physics-Based Calculation and Optimization), not just a size sort.
     Candidates with no loss data at all fall back to area-product-ascending
     so missing data never silently wins or loses a comparison; area product
     is always the tiebreaker. Everything else goes to rejectedCandidates
     with every failed check listed
↓
Route serializes the returned DesignRecommendation to JSON and responds
↓
Frontend renders candidates, rejected candidates (with rejection reasons and
missing-data warnings), and the active DesignRules - no field is a hidden
assumption.
```

---

## Recommendation Confidence, Bottleneck Analysis, and Ranking Highlights

Five additive fields on top of the existing pipeline (nothing about the turns/gap solver,
validation checks, or the primary candidate sort changes) - added to make the engine's existing
honesty mechanisms visible, not to add new ones.

**Mode 2 peak-current derivation** (`RequirementDerivationService.cpp`): when RMS current and
ripple current are both supplied but peak current is not, the engine now derives a real peak
current from them - `Iavg = sqrt(Irms^2 - ripple^2/12)`, `peak = Iavg + ripple/2` - the algebraic
inverse of the same triangular-ripple formula already used elsewhere in this file for the
average+ripple → RMS direction, not a new assumption. `OperatingPoint::peakCurrentDerived`/
`peakCurrentAssumption` (mirroring `rmsCurrentDerived`/`rmsCurrentAssumption`) make sure this is
always labeled as derived, never presented as a directly measured value. When the derivation is
mathematically impossible for a triangular waveform (`Irms^2 < ripple^2/12`), it never throws -
`peakCurrentA` simply stays absent, matching this file's existing soften-don't-crash policy.

**`BottleneckAnalysis`** (`core/model/BottleneckAnalysis.h`, `BottleneckAnalysisService`):
identifies the single check most responsible for a candidate's current standing - the failed check
closest to passing (rejected candidates), the mandatory check that couldn't run at all because
required data wasn't supplied (a data-completeness gap, always reported ahead of margin headroom -
"we don't know" outranks "it has margin on what we could check"), or the evaluated check with the
smallest normalized margin (`margin / limitValue`, since raw margins across checks have
incommensurable units - %, T, °C, mm, A/mm²). `CurrentConsistencyValidation` is excluded from
every margin comparison (diagnostic, not a physical gate - it cannot fail).

**`suggestImprovement()`** (`RankingExplanationService.cpp`): a rule-based mapping from
`candidate.bottleneck` to a short engineering suggestion, stored in the new
`InductorCandidate::designNarrative` field (distinct from `rankingExplanation`, which answers "why
did this rank here," not "what would improve it"). When the bottleneck is specifically missing
peak current, always returns the exact phrase `"supply peak current to evaluate saturation risk"` -
never a generic guess when the real blocker is a missing input.

**`RankingHighlights`** (`core/model/RankingHighlights.h`, `RankingHighlightsService`): among the
already-sorted, already-passing `recommendation.candidates` list, identifies the best-in-category
core (by `partNumber`, not list index - an index would silently break under any future re-sort or
pagination) for thermal rise, known partial loss, saturation margin, and area product. Purely
additive - `candidateRanksAhead()`'s own sort order and tier logic are untouched.

**A fact this round surfaced rather than fixed**: `RecommendationTier::Pass` (the top tier) is
already *structurally unreachable* in Phase 1 - `ThermalValidation` sets `isPreliminaryEstimate=true`
on every result it ever produces, since no fully-validated thermal model exists yet, and `Pass`
requires every mandatory check to rest on measured (never preliminary/default) data. This is
documented directly in `RecommendationStatus.h` and is a correct, honest constraint - every real
candidate this engine produces today is capped at `ConditionalPass` at best, which is exactly why
`BottleneckAnalysis` exists: to make clear *why* a candidate is capped there, not to pretend the cap
doesn't exist.

---

## Powder-Core DC-Bias Permeability Roll-Off

A real, previously-missing piece of physics for distributed-gap (powder) toroids - `TurnsAndGapDesign.cpp`
used to solve `N = round(sqrt(L/AL0))` once against the core's catalog AL and call it done, reporting the
exact same inductance at 0 A and at the request's real operating current. Real powder cores (MPP, Kool Mµ,
High Flux, XFlux, Edge, Mix) don't work that way - permeability rolls off as DC bias current rises, so the
true inductance at current is measurably below the 0-bias catalog number. A real user report ("at 0 you get
3mH and at 5 amps you should see less than 3mH") caught this.

**`data/dc_bias_curves.csv` / `DCBiasCurveDatabase.h`**: real, manufacturer-published curve-fit coefficients
(`a`, `b`, `c`, `d`) for `%initial_permeability = 1/(a + b*H^c) + d` (H in Oersteds), transcribed directly
from Magnetics Inc.'s and Micrometals' own "Permeability vs. DC Bias" datasheet pages (Toroid shape family)
- not derived, fitted, or estimated by this project. Currently covers six materials (MPP 60, Kool Mµ 60,
High Flux 14, High Flux 26, XFlux 60, Edge 60, Mix 26); a distributed-gap material with no row here falls
back to the original flat-AL0 behavior, never a guessed curve.

**`core/magnetics/PermeabilityRolloff.h/.cpp`**: `findDCBiasCurve()` (lookup, mirrors
`findCoreLossCoefficients()`'s pattern), `dcMagnetizingForceOe()` (`H = 0.4*pi*N*I/le`, le in cm - the
standard formula, confirmed against Magnetics Inc.'s own published worked example), and
`percentInitialPermeability()` (the formula above, clamped to [0, 100]). Both formula and transcribed
coefficients are cross-checked against real datasheet spot-values in `PermeabilityRolloffTests.cpp` (a 60µ
Kool Mµ core at H=57.5 Oe should read ~72% per Magnetics Inc.'s own worked example; Mix-26 at 50 Oe should
bracket Micrometals' stated 47.4%/55.2% min/nom).

**`TurnsAndGapDesign.cpp`'s distributed-gap branch**: when a real curve exists for the material *and* a real
operating current is known (peak if supplied, else RMS as the same guaranteed-lower-bound floor already
established for the ferrite flux-aware seed), turns and the rolled-off AL are solved together as a fixed
point - seed turns from AL0, compute H, look up %µ, get `AL_eff = AL0 * %µ/100`, recompute the turns
required against `AL_eff`, repeat (capped at the same `kMaxIterations` the ferrite loop uses). This map is
monotonically increasing in turns (more turns raises H, which lowers %µ, which demands still more turns), so
a genuine "no turns count on this core achieves the target L at this current" case - operating near the
material's real saturation behavior - fails to converge and is rejected with a real reason, never forced to
a number. `TurnsAndGapResult` gained four fields to make this visible: `usesDCBiasRolloffCurve`,
`dcMagnetizingForceOe`, `percentInitialPermeabilityAtOperatingCurrent`, `dcBiasRolloffUsedRmsFloor` - all
false/100%/0 (the old behavior, unchanged) whenever no curve or no current is available.

**Shape no longer gates which cores are distributed-gap.** `isDistributedGapCore` originally also required
`coreShape=="Toroid"`, inherited from the original ferrite-toroid fix (real ferrite toroids like N87 need
the machined-gap formula despite being toroids). That condition was backwards for powder E-cores/blocks/
EQ/LP families (e.g. Kool Mµ E/U/EER, XFlux Blocks) - they're just as distributed-gap as powder toroids,
same material, same physics, only the mechanical shape differs - but were wrongly running the ferrite
machined-gap formula, producing physically nonsensical turns counts (400-700+) and >1.0 winding-fill
factors. A real user report (3000µH/5A RMS/100kHz across all materials) caught this. The fix: shape was
never the right signal in either direction - only `real_cores.csv`'s own `MaterialType` column
("ferrite"/"powder") is. `isDistributedGapCore` is now simply `core.materialType == "powder"`.

**The area-product pre-filter was excluding most powder cores before the real solve ever ran.**
`CoreEvaluation.cpp`'s `findSuitableCores()` gates candidates on `meetsAreaProduct`, computed from
`calculateAp()` (McLyman's `Ap = 2E*1e4/(Ku*Bmax*J)`) *before* `TurnsAndGapDesign.cpp`'s real turns/gap/
DC-bias solve ever runs on them. That formula assumes gap is a free variable - a fair shortcut for gapped
ferrite ("is this core physically big enough at Bmax, since any AL can be reached by adjusting the gap"),
but the wrong question for powder cores, whose real constraint is whether enough turns exist that the real
roll-off-corrected AL still reaches the target inductance - something this closed-form estimate has no
notion of. A real user report found this meant 61 of 80 powder cores in the database never reached real
per-candidate evaluation at all when a peak current was supplied. Same fix philosophy as the pre-existing
`peakSupplied==false` skip in the same function: when a shortcut can't be trusted for a case, skip it and
let the real per-candidate check decide. `meetsAreaProduct` is now unconditionally `true` for any core
with `materialType=="powder"`, judged only by the real turns/gap/DC-bias solve and the real
`DesignValidation` checks downstream - never pre-excluded by a formula that doesn't model how powder
cores actually work.

---

## Data Source: Real Data, Hand-Curated from Magnetics Inc. — No Third-Party Library

`cores.csv` and `materials.csv` (the old hand-typed placeholder files) are
gone for good. In their place: **`data/real_materials.csv`** and
**`data/real_cores.csv`** — also plain CSVs, but their *contents* are real,
sourced data. As of the current snapshot, every row is transcribed by hand
from Magnetics Inc.'s own live catalog (mag-inc.com's Advanced Part Number
Finder) — MPP powder toroids and Magnetics E-cores only (755 cores, 34
materials). There is no PyOpenMagnetics (or any other third-party magnetics
library) dependency anywhere in this project, at build time or runtime —
an earlier version of this project sourced data that way, found real errors
in the sampled result for this project's actual scope (missing permeability
grades, mismatched catalog numbers), and replaced it entirely.
(`reference_designs.csv` and `test_scenarios.csv` are unrelated to any of
this and are still used, for validation.)

**The architecture:**
1. `data/real_materials.csv` / `data/real_cores.csv` / `data/dc_bias_curves.csv` /
`data/real_core_loss_coefficients.csv` are **hand-curated, not generated** —
there is no script that regenerates them. To change what's in them, edit
the CSVs directly, sourcing any new/changed values from Magnetics Inc.'s own
live catalog by hand.
2. At FastAPI startup (`python/app.py`, `load_real_magnetics_data()`),
`python/services/magnetics_data.py` reads those files — plain file I/O,
no external library import, works natively on any platform Python runs
on — and maps them into the `CoreData`/`MaterialData` shape the C++ engine
expects.
3. `magnetics_cpp.set_core_database(...)` / `set_material_database(...)`
hand that data to `CoreDatabase`/`Materials` in C++, once, cached in
memory for the life of the process — not re-read per request.
4. **If any step fails, startup still fails.** `load_real_magnetics_data()`
raises rather than catching the error — uvicorn refuses to start rather
than run with an empty core/material database.

**Trade-off, stated plainly:** this data is a snapshot, not live. If
Magnetics' catalog changes, someone has to re-pull the affected rows by
hand from mag-inc.com the same way this snapshot was built — there is no
automated refresh mechanism.

The actual Ap-based selection logic (originally `CoreSelection.cpp`/
`MaterialSelection.cpp`, now `src/core/sizing/CoreEvaluation.cpp`/`MaterialEvaluation.cpp`
after the Phase 1 rewrite) did not change through any of this. Only where
the candidate list comes from changed: hand-typed CSV → third-party-library
sample → hand-curated Magnetics Inc. snapshot.

---

## Build Configuration

`CMakeLists.txt` defines:
- `magnetics_engine` — static library: all of `src/core/` + `src/validation/` + `src/rules/` + `src/data/`
- `magnetics_services` — static library: `src/backend/services/`, links against `magnetics_engine`
- `magnetics_cpp` — the pybind11 extension module (`src/python_bindings/InductorDesignBindings.cpp`), links against `magnetics_services`, output directed into `python/`
- `magnetics_engine_tests` — a small `assert()`-based executable (`tests/cpp/EngineTests.cpp`), registered with `add_test()`; run via `ctest`
- `VALIDATION_SOURCES` and `RULES_SOURCES` are no longer empty — `validation/DesignValidation.cpp` (using the `ValidationResult` struct declared in `Validation.h`) and `rules/DesignRules.cpp` are compiled into `magnetics_engine`

There is no `magnetics_server` target — that name (and the standalone-C++-HTTP-server approach) belongs to an earlier plan and no longer reflects the build.

---

## Key Design Points

- **Separation of concerns:** HTTP parsing (FastAPI routes) is separate from business orchestration (Services) is separate from math (Core Engine). The Core Engine has no knowledge of HTTP or Python.
- **Why FastAPI + pybind11 instead of a hand-written server:** request/response validation comes for free from Pydantic; FastAPI auto-generates interactive docs at `/docs`; no socket-handling code to write or maintain.

---

## Future Extensibility

To add a new feature (e.g., temperature-corrected core loss using the coefficient rows' ct0/ct1/ct2 terms, once their formula is confirmed):
1. **Implement in Core Engine:** write/extend the relevant `src/core/*.cpp` module
2. **Expose via bindings:** add the function/struct to `src/python_bindings/InductorDesignBindings.cpp`
3. **Wire into the orchestrator:** call it from `src/backend/services/InductorDesignService.cpp`
4. **Update frontend:** add the new field(s) in `app.js` (the recommendation is walked generically from `DesignRecommendation`, so most new fields just need a render line)
5. **Rebuild:** re-run the CMake build for `magnetics_cpp`, restart uvicorn

Buck converter requirement derivation (Mode 1, `BuckElectricalSolver`) is
implemented; Boost/Buck-Boost/Flyback are not. Adding one follows the same
pattern as adding Buck did — a new `TopologyInput`-shaped struct (or an
extra `topology` case if the fields overlap enough to share one), a new
`*ElectricalSolver` outputting the existing `InductorDesignRequest`, one
new binding, and one new route (`/topology-design/boost`, etc.) — none of
it touches `InductorDesignService` or anything downstream of Mode 1/Mode 2
convergence.

---

## Dependencies

- **pybind11** — C++/Python interop
- **FastAPI + uvicorn** — HTTP layer
- **Pydantic** — request/response validation (comes with FastAPI)
- **std::vector, std::string** — C++ standard library
- No PyOpenMagnetics or any other third-party magnetics data library — data is hand-curated directly from Magnetics Inc.'s catalog, see "Data Source" below
- No other external C++ libraries

---

## Deployment

**Local:** single process — `uvicorn` runs the FastAPI app, which serves both the static frontend and the API, all backed by the in-process `magnetics_cpp` extension. No separate server processes.

**Production:** deployed to Vercel (`vercel.json` routes everything to `python/app.py` via `@vercel/python`) — same single-process model, but Vercel's Python builder has no CMake step, so `magnetics_cpp` can't be compiled during deployment. A prebuilt `.so` (`python/magnetics_cpp.cpython-312-x86_64-linux-gnu.so`) is checked into git as a `.gitignore` exception and is the only reason the import succeeds there. **This means the live site is not automatically in sync with `src/` changes** — any C++ engine change requires a separate rebuild-and-commit of that binary, or Vercel keeps serving the old engine logic under the new frontend indefinitely with no error. See [GETTING_STARTED.md](GETTING_STARTED.md)'s "Deploying to Vercel" section for the exact rebuild recipe (must use g++-11, not a newer GCC) and for the real incident this already caused once.