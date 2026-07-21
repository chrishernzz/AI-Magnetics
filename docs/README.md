# AIMagnetics
**Engineer-focused power inductor sizing tool** — topology-agnostic (buck, boost, flyback output inductor, etc.), following the area-product (Ap) method from McLyman, *Transformer and Inductor Design Handbook*, p.63.

## What It Does
Given your inductor requirements (inductance, peak/RMS current, switching frequency, temperature limits), AIMagnetics runs a full Phase 1 single-winding power inductor design pipeline: material candidates → area product → core candidates → turns + air-gap design → magnetic validation (inductance, peak flux, saturation, winding fit, current density, thermal) → round-wire winding design → DC copper loss. Every candidate's pass/fail is explainable, with rejection reasons and missing-data warnings rather than a single silent pick — see Status below for what's fully evaluated vs. not_evaluated pending more data.

**Input:** `InductorDesignRequest` — inductance, peak current, RMS current (or derivable from average current + ripple), switching frequency, ambient temperature, allowable temperature rise, inductance tolerance (see [API_REFERENCE.md](API_REFERENCE.md))
**Output:** `POST /inductor-design` returns a `DesignRecommendation` — passing and rejected candidates, each with material, core, turns/gap, validation results, winding design, and losses; or `status: "no_feasible_design"` with the reason, never a silent oversized fallback
**Known Phase 1 gaps:** DC copper loss is now evaluated with a real, geometry-derived DCR for cores with mean-length-per-turn data (see [DATA_FILES.md](DATA_FILES.md)). Core loss is still `not_evaluated` — real Steinmetz coefficients exist in `data/real_core_loss_coefficients.csv` and are loaded and searchable (`findCoreLossCoefficients()`), but `CoreLoss.cpp`'s loss formula isn't wired to use them yet; this is now purely a code gap, not a data gap.

---
## Quick Links

| I want to... | Read this |
|---|---|
| **Build and run the tool** | [GETTING_STARTED.md](GETTING_STARTED.md) |
| **Understand the design method** | [WORKFLOW.md](WORKFLOW.md) |
| **Understand every formula and why it's used** | [FORMULAS.md](FORMULAS.md) |
| **See the system architecture** | [ARCHITECTURE.md](ARCHITECTURE.md) |
| **Use the web UI** | [FRONTEND_GUIDE.md](FRONTEND_GUIDE.md) |
| **Call the API** | [API_REFERENCE.md](API_REFERENCE.md) |
| **Add new cores or materials** | [DATA_FILES.md](DATA_FILES.md) |
| **Requirements traceability** | [REQUIREMENTS.md](REQUIREMENTS.md) |
| **Worked example / manual test** | [TESTRESULTSMEAN.md](TESTRESULTSMEAN.md) |
| **Day-to-day run commands** | [WHEN_TO_RUN_PROGRAM.md](WHEN_TO_RUN_PROGRAM.md) |

---
## The Design Workflow (Phase 1)
1. **Material candidates** — every material whose frequency range covers the request (not just the first match)
2. **Area Product (Ap)** — minimum core size needed without overheating, using the named `DesignRules::phase1Default()` ruleset (Ku, Bmax, J) — never hard-coded in the route layer
3. **Core candidates** — every core matching a compatible material; `no_feasible_design` (not a silent oversized fallback) if none meet the Ap requirement
4. **Turns and air-gap design** — iterates turns and gap together until inductance converges within tolerance
5. **Magnetic validation** — inductance, peak flux, saturation margin, winding fit, current density, thermal (six named checks, all failures reported)
6. **Winding design** — round-wire AWG selection, fill factor, current density, DCR from a real geometry-derived mean-length-per-turn (see Status)
7. **Loss evaluation** — DC copper loss when DCR is available; core loss and high-frequency loss reported `not_evaluated` (see Status)

See [WORKFLOW.md](WORKFLOW.md) for formulas and the current status of each stage.

---
## Technology Stack
- **Core Engine:** C++17 (`magnetics_engine`, `magnetics_services` — CMake static libraries)
- **Python Bindings:** pybind11 (`magnetics_cpp` module, built by CMake)
- **Backend:** FastAPI (Python), run via `uvicorn`
- **Frontend:** Vanilla JavaScript + HTML/CSS
- **Build:** CMake 3.16+
- **Data:** CSV files (cores, materials, reference designs, test scenarios)

---

## Status
- ✅ Material candidate evaluation (`src/core/sizing/MaterialEvaluation.cpp` — `findSuitableMaterials()`, replaces the old single-pick)
- ✅ Area product calculation, sourced from the named `DesignRules::phase1Default()` ruleset
- ✅ Core candidate evaluation (`src/core/sizing/CoreEvaluation.cpp` — `findSuitableCores()`); no silent oversized fallback — returns `no_feasible_design` with the required/largest-available area product instead
- ✅ Gap design (`src/core/magnetics/GapDesign.cpp`) and turns/gap convergence (`src/core/magnetics/TurnsAndGapDesign.cpp`) — implemented and iterated together
- ✅ Magnetic validation (`DesignValidation.cpp`) — InductanceValidation, PeakFluxValidation, SaturationValidation, WindingFitValidation, CurrentDensityValidation, ThermalValidation
- ✅ Winding design (`src/core/winding/WindingDesign.cpp`) — AWG wire selection, fill factor, current density
- ✅ DC copper loss (`src/core/losses/CopperLoss.cpp`), called from `src/core/losses/LossEvaluation.cpp` — real DCR from `CoreCandidate.mltMm` (see [DATA_FILES.md](DATA_FILES.md)), `not_evaluated` only for the subset of cores whose upstream geometry doesn't support an MLT estimate
- ✅ Saturation flux density (`BmaxT`) — real, material-specific data for all 32 materials in the current snapshot; `SaturationValidation`/`PeakFluxValidation` use it automatically instead of the Phase 1 default
- ⚠️ Core loss (`src/core/losses/CoreLoss.cpp`) — still `not_evaluated`: real Steinmetz coefficients now exist in `data/real_core_loss_coefficients.csv`, loaded at startup and searchable via `findCoreLossCoefficients()`, but the loss-density formula itself still uses its old simplified placeholder model, unused, and flux-density swing isn't threaded into `src/core/losses/LossEvaluation.cpp` either (see [FORMULAS.md](FORMULAS.md) section 9)
- ⚠️ High-frequency (skin/proximity) loss — not implemented in Phase 1, reported `not_evaluated`
- ⚠️ Thermal evaluation (`src/core/thermal/ThermalEvaluation.cpp`) — `not_evaluated`: no thermal-resistance model or data yet
- ✅ `src/core/magnetics/TurnsCalculation.cpp` is fully implemented (`N = round(sqrt(L/AL))`) — this was previously mis-documented as a stub in several files; it's used as the seed estimate inside `src/core/magnetics/TurnsAndGapDesign.cpp`'s convergence loop
- ✅ Automated tests — `ctest` (C++ unit-conversion + gap/AL formula checks) and `pytest tests/python` (scenario/reference-design checks against `data/test_scenarios.csv` / `data/reference_designs.csv`, with a documented `xfail` for the core-part-number data mismatch — see [DATA_FILES.md](DATA_FILES.md))

---
## Resources
- **Design Reference:** McLyman, *Transformer and Inductor Design Handbook*
- **Data Format:** See [DATA_FILES.md](DATA_FILES.md) for CSV structure
- **Vendor References:** Core geometry sourced from real manufacturer data (Ferroxcube, TDK, Magnetics, Fair-Rite, and others), originally via PyOpenMagnetics/MAS, bundled as a snapshot in `data/real_cores.csv` (see [ARCHITECTURE.md](ARCHITECTURE.md)); one reference design (`i77006`, an IntelliPower part) used for validation