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

**Location:** `src/frontend/`

| File | Purpose |
|---|---|
| `index.html` | Web page structure; form for requirements input |
| `app.js` | Handles form submission, calls the API, renders results |
| `styles.css` | Styling for the UI |

**Endpoints it calls** (relative paths, no `/api` prefix):
- `POST /material-selection`
- `POST /calculate` (area product)
- `POST /core-selection`

All three requests send the same payload shape: `{ inductanceUH, peakCurrentA, switchingFreqKHz, allowableTempRiseC }`.

---

### Backend Layer — FastAPI (Python)

**Location:** `python/`

| File | Purpose |
|---|---|
| `app.py` | Creates the FastAPI app, mounts `/static` for the frontend, serves `index.html` at `/`, includes the router |
| `routes/core_selection.py` | Defines the three POST endpoints; builds C++ input structs and calls into `magnetics_cpp` |

**Routes defined** (all in `routes/core_selection.py`, no path prefix applied to the router):
| Route | Calls into C++ | Returns |
|---|---|---|
| `POST /material-selection` | `MaterialSelectionService().calculate(...)` | `MaterialSelectionResponse` |
| `POST /calculate` | `magnetics_cpp.calculate_ap(...)`, `calculate_stored_energy(...)` | `AreaProductResponse` |
| `POST /core-selection` | `CoreSelectionService().calculate(...)` (internally also calls material selection again) | `CoreSelectionResponse` |
| `GET /` | — | `index.html` (via `FileResponse`) |
| `GET /static/*` | — | frontend static files (mounted via `StaticFiles`) |

Request/response validation is handled by Pydantic models declared directly in `routes/core_selection.py` — there are no separate "Controller" classes; the route function bodies do the parsing, calling, and response-shaping in one place.

---

### Python Bindings Layer — pybind11

**Location:** `src/python_bindings/CoreSelectionBindings.cpp`

Builds a single pybind11 module, `magnetics_cpp`, exposing:
- `MaterialSelectionInput` / `MaterialSelectionResult` / `MaterialSelectionService`
- `AreaProductInput`, `calculate_ap()`, `calculate_stored_energy()`
- `CoreSelectionInput` / `CoreSelectionResult` / `CoreSelectionService`

CMake builds this as a Python extension module and places the compiled `.pyd`/`.so` directly in `python/`, so `import magnetics_cpp` resolves it as a local module.

---

### Core Engine Layer

**Location:** `src/core/`, `src/backend/services/`, `src/data/`

| Module | File | Status |
|---|---|---|
| MaterialSelection | `MaterialSelection.cpp` | ✅ Implemented |
| AreaProduct | `AreaProduct.cpp` | ✅ Implemented |
| CoreSelection | `CoreSelection.cpp` | ✅ Implemented |
| GapDesign | `GapDesign.cpp` | ❌ Stub (commented out) |
| TurnsCalculation | `TurnsCalculation.cpp` | ❌ Stub (commented out) |
| CopperLoss | `CopperLoss.cpp` | ❌ Stub (commented out) |
| CoreLoss | `CoreLoss.cpp` | ❌ Stub (commented out) |
| HighFrequencyLosses | `HighFrequencyLosses.cpp` | ❌ Stub (hard-coded return 0.0) |
| DataCache<T> | `data/DataCache.h` | ✅ Implemented — shared template holding the "cache once, warn if empty" logic that `CoreDatabase` and `Materials` both use, instead of each repeating it |
| CoreDatabase | `data/CoreDatabase.h` | ✅ Implemented — real data loaded once at startup from a bundled snapshot (`data/real_cores.csv`, sourced from PyOpenMagnetics — see "Data Source" below); no CSV fallback, startup fails loudly if this fails. `load()` returns by `const&`, not by value |
| Materials | `data/Materials.h` | ✅ Implemented — same pattern as CoreDatabase |
| TurnsCalculation | `core/TurnsCalculation.cpp` | ✅ Implemented — N = sqrt(L / AL), verified against the real i77006 reference design (65 computed vs. 64 actual) |
| Validation | `src/validation/Validation.h` | ❌ Header only — `ValidationResult{bool passed}` struct, no `.cpp`, not in `CMakeLists.txt` |
| DesignRules | `src/rules/DesignRules.h` | ❌ Header only — no `.cpp`, not in `CMakeLists.txt` |

The **Services** layer (`src/backend/services/`) sits directly between the pybind11 bindings and the core engine — e.g. `CoreSelectionService::calculate()` calls `selectCore()` from `CoreSelection.cpp`. There is no separate "Controller" layer in C++; HTTP parsing happens entirely in the Python route functions.

---

## Data Flow: From User Input to Result

```
User enters: L=250µH, Ipk=5A, f=100kHz, ΔT=40°C
↓
Frontend POSTs to /material-selection with {inductanceUH, peakCurrentA, switchingFreqKHz, allowableTempRiseC}
↓
FastAPI route builds a MaterialSelectionInput, calls magnetics_cpp.MaterialSelectionService().calculate(...)
↓
C++ engine looks up material from its in-memory database (loaded once at
startup from a bundled real-data snapshot — see "Data Source" below), matches frequency range
↓
Route returns MaterialSelectionResponse: {materialFamily, muOpt, reason, alternatives}
↓
Frontend displays material, then POSTs to /calculate with the same payload
↓
Route builds AreaProductInput (Ku=0.4, Bmax=0.30T, J=400 A/cm² are hard-coded here),
calls magnetics_cpp.calculate_ap() and calculate_stored_energy()
↓
Returns AreaProductResponse: {areaProduct, energy}
↓
Frontend POSTs to /core-selection with the same payload
↓
Route re-runs material selection internally, builds CoreSelectionInput,
calls magnetics_cpp.CoreSelectionService().calculate(...)
↓
C++ engine looks up cores from its in-memory database (same startup-loaded
real data), filters by Ap + material, ranks by loss heuristic
↓
Returns CoreSelectionResponse: {partNumber, material, mu, al, ae, wa, le}
↓
Frontend renders material, core, energy/Ap details, and a summary panel
whose "Next Step" field literally says "Turns & Loss Design" — the UI
already acknowledges Stage 4 isn't built yet.
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
Windows via WSL2 — see its own `docs/compatibility.md`), and has no
published wheel for Python 3.14 on any platform. Since this project runs
on native Windows with Python 3.14, importing PyOpenMagnetics at runtime
isn't currently possible here — confirmed by an actual failed install
(`pip install PyOpenMagnetics` → CMake configuration failed, source build
attempted because no matching wheel exists).

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

`CoreSelection.cpp`/`MaterialSelection.cpp` — the actual Ap-based selection
logic — did not change through any of this. Only where the candidate list
comes from changed, twice now: hand-typed CSV → live PyOpenMagnetics query
→ bundled real-data snapshot.

---

## Build Configuration

`CMakeLists.txt` defines:
- `magnetics_engine` — static library: all of `src/core/` + `src/data/`
- `magnetics_services` — static library: `src/backend/services/`, links against `magnetics_engine`
- `magnetics_cpp` — the pybind11 extension module, links against `magnetics_services`, output directed into `python/`
- `VALIDATION_SOURCES` and `RULES_SOURCES` are both **empty lists** — `Validation.h` and `DesignRules.h` are declared but not compiled into anything yet

There is no `magnetics_server` target — that name (and the standalone-C++-HTTP-server approach) belongs to an earlier plan and no longer reflects the build.

---

## Key Design Points

- **Separation of concerns:** HTTP parsing (FastAPI routes) is separate from business orchestration (Services) is separate from math (Core Engine). The Core Engine has no knowledge of HTTP or Python.
- **Why FastAPI + pybind11 instead of a hand-written server:** request/response validation comes for free from Pydantic; FastAPI auto-generates interactive docs at `/docs`; no socket-handling code to write or maintain.

---

## Future Extensibility

To add a new feature (e.g., turns calculation):
1. **Implement in Core Engine:** write the real body of `src/core/TurnsCalculation.cpp` (currently a stub)
2. **Expose via bindings:** add the function/struct to `src/python_bindings/CoreSelectionBindings.cpp`
3. **Add a FastAPI route:** new endpoint in `python/routes/core_selection.py` (or a new routes file, included in `app.py`)
4. **Update frontend:** add the new field(s) in `app.js` and `index.html`
5. **Rebuild:** re-run the CMake build for `magnetics_cpp`, restart uvicorn

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

**Current model:** single process — `uvicorn` runs the FastAPI app, which serves both the static frontend and the API, all backed by the in-process `magnetics_cpp` extension. No separate server processes.