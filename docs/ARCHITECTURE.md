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
| ThermalEvaluation | `src/core/thermal/ThermalEvaluation.cpp` | ✅ Implemented — real iterative convergence loop (temp → hot DCR → copper loss → temp rise → repeat) using `DesignRules.defaultThermalResistanceCPerW` (a Phase 1 default, never per-core data). Caps at `PreliminaryThermalEstimate` — `ThermalStatus` has no "fully evaluated" value. `NotEvaluated` when DCR geometry is unknown or the loop's real positive-feedback iteration diverges |
| RecommendationStatus | `src/validation/RecommendationStatus.cpp` | ✅ Implemented — 3-tier `Pass`/`ConditionalPass`/`Reject` classification replacing the old frontend-only "Recommended" sugar; `Pass` currently unreachable in practice (ThermalValidation always flags isPreliminaryEstimate) |
| DataCache<T> | `src/data/DataCache.h` | ✅ Implemented — shared template holding the "cache once, warn if empty" logic that `CoreDatabase`, `MaterialDatabase`, and `CoreLossCoefficientDatabase` all use, instead of each repeating it |
| CoreDatabase | `src/data/CoreDatabase.h` | ✅ Implemented — real data loaded once at startup from a bundled snapshot (`data/real_cores.csv`, sourced from PyOpenMagnetics — see "Data Source" below); no CSV fallback, startup fails loudly if this fails. `load()` returns by `const&`, not by value |
| MaterialDatabase | `src/data/MaterialDatabase.h` | ✅ Implemented — same pattern as CoreDatabase; `MaterialData` now also carries a real, material-specific `bmaxT` for all 81 materials |
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
     together), then DesignValidation's six checks, designWinding(),
     evaluateLosses(), evaluateThermal()
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

## Data Source: Real Data, Bundled Snapshot — No Live Windows Dependency

`cores.csv` and `materials.csv` (the old hand-typed files) are gone for
good. In their place: **`data/real_materials.csv`** and **`data/real_cores.csv`**
— also plain CSVs, but their *contents* are real, sourced data (from
PyOpenMagnetics/MAS — real manufacturer datasheets: Ferroxcube, TDK,
Magnetics, Fair-Rite), not hand-typed. (`reference_designs.csv` and
`test_scenarios.csv` are unrelated to any of this and are still used, for
validation.)

**Why a bundled snapshot instead of a live PyOpenMagnetics query:**
PyOpenMagnetics does not support native Windows (Linux/macOS only, or
Windows via WSL2 — see its own `docs/compatibility.md`), and had no
published wheel for Python 3.14 on any platform at the time. This is
historical context for *why* the snapshot approach was chosen, not a
statement about the current toolchain — the project is now pinned to
Python 3.12 (`.python-version`) and deploys on Linux (Vercel), where
PyOpenMagnetics does install; the original blocker was hit on native
Windows with Python 3.14 during early development, confirmed by an actual
failed install (`pip install PyOpenMagnetics` → CMake configuration
failed, source build attempted because no matching wheel exists).

**The architecture that resulted, and why it still meets the same bar as
a live query:**
1. `scripts/export_real_data.py` — a **maintenance script, not part of the
running app** — queries PyOpenMagnetics for real materials/cores,
applies the same filters as before (power-application only, ungapped
only, spread across materials/vendors), and writes the result to
`data/real_materials.csv` / `data/real_cores.csv`. This only runs
somewhere PyOpenMagnetics actually installs (Linux, macOS, WSL2) —
never as part of starting the app.
2. At FastAPI startup (`python/app.py`, `load_real_magnetics_data()`),
`python/services/magnetics_data.py` reads those two CSV files —
**plain file I/O, no PyOpenMagnetics import, works natively on
Windows** — and maps them into the same `CoreData`/`MaterialData`
shape the C++ engine has always used.
3. `magnetics_cpp.set_core_database(...)` / `set_material_database(...)`
hand that data to `CoreDatabase`/`Materials` in C++, once, cached in
memory for the life of the process — not re-read per request.
4. **If any step fails, startup still fails.** `load_real_magnetics_data()`
still raises rather than catching the error — uvicorn refuses to start
rather than run with an empty database. This didn't change; only where
the data comes from changed.

**Trade-off, stated plainly:** this data is a snapshot from whenever
`export_real_data.py` was last run, not live-queried on every startup.
Refreshing it is a manual step (re-run the script somewhere PyOpenMagnetics
installs, replace the two CSV files) rather than automatic. Given native
Windows can't run the live version at all, this is the trade made to have
real data working here at all.

The actual Ap-based selection logic (originally `CoreSelection.cpp`/
`MaterialSelection.cpp`, now `src/core/sizing/CoreEvaluation.cpp`/`MaterialEvaluation.cpp`
after the Phase 1 rewrite) did not change through any of this. Only where
the candidate list comes from changed, twice now: hand-typed CSV → live
PyOpenMagnetics query → bundled real-data snapshot.

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
- **PyOpenMagnetics** — used only by `scripts/export_real_data.py` (a maintenance script, run manually, somewhere it actually installs) to regenerate the real-data snapshot. NOT a runtime dependency of the running app — see "Data Source" below for why.
- No other external C++ libraries

---

## Deployment

**Local:** single process — `uvicorn` runs the FastAPI app, which serves both the static frontend and the API, all backed by the in-process `magnetics_cpp` extension. No separate server processes.

**Production:** deployed to Vercel (`vercel.json` routes everything to `python/app.py` via `@vercel/python`) — same single-process model, but Vercel's Python builder has no CMake step, so `magnetics_cpp` can't be compiled during deployment. A prebuilt `.so` (`python/magnetics_cpp.cpython-312-x86_64-linux-gnu.so`) is checked into git as a `.gitignore` exception and is the only reason the import succeeds there. **This means the live site is not automatically in sync with `src/` changes** — any C++ engine change requires a separate rebuild-and-commit of that binary, or Vercel keeps serving the old engine logic under the new frontend indefinitely with no error. See [GETTING_STARTED.md](GETTING_STARTED.md)'s "Deploying to Vercel" section for the exact rebuild recipe (must use g++-11, not a newer GCC) and for the real incident this already caused once.