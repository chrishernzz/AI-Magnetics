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
| CoreDatabase | `data/CoreDatabase.cpp` | ✅ Implemented — loads `cores.csv` |
| Materials | `data/Materials.cpp` | ✅ Implemented — loads `materials.csv` |
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
C++ engine loads materials.csv, matches frequency range (e.g. Powder Iron)
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
C++ engine loads cores.csv, filters by Ap + material, ranks by loss heuristic
↓
Returns CoreSelectionResponse: {partNumber, material, mu, al, ae, wa, le}
↓
Frontend renders material, core, energy/Ap details, and a summary panel
whose "Next Step" field literally says "Turns & Loss Design" — the UI
already acknowledges Stage 4 isn't built yet.
```

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
- **CSV parsing** — hand-rolled in `CoreDatabase.cpp`, `Materials.cpp`
- No other external C++ libraries

---

## Deployment

**Current model:** single process — `uvicorn` runs the FastAPI app, which serves both the static frontend and the API, all backed by the in-process `magnetics_cpp` extension. No separate server processes.