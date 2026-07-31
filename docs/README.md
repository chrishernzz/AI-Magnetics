# AIMagnetics
**Engineer-focused power inductor sizing tool** — topology-agnostic (buck, boost, flyback output inductor, etc.), following the area-product (Ap) method from McLyman, *Transformer and Inductor Design Handbook*, p.63.

## What It Does
Given your inductor requirements (inductance, peak/RMS current, switching frequency, temperature limits), AIMagnetics runs a full Phase 1 single-winding power inductor design pipeline: material candidates → area product → core candidates → turns + air-gap design → magnetic validation (inductance, peak flux, saturation, winding fit, current density, thermal) → round-wire winding design → DC copper loss. Every candidate's pass/fail is explainable, with rejection reasons and missing-data warnings rather than a single silent pick — see Status below for what's fully evaluated vs. not_evaluated pending more data.

**Input:** `InductorDesignRequest` — inductance, peak current, RMS current (or derivable from average current + ripple), switching frequency, ambient temperature, allowable temperature rise, inductance tolerance (see [API_REFERENCE.md](API_REFERENCE.md))
**Output:** `POST /inductor-design` returns a `DesignRecommendation` — passing and rejected candidates, each with material, core, turns/gap, validation results, winding design, and losses; or `status: "no_feasible_design"` with the reason, never a silent oversized fallback
**Known Phase 1 gaps:** DC copper loss is now evaluated with a real, geometry-derived DCR for cores with mean-length-per-turn data (see [DATA_FILES.md](DATA_FILES.md)). Core loss is now a real, computed value (`Pv = k*f^alpha*B^beta`) whenever the material has Steinmetz coefficients AND the request supplies `rippleCurrentPeakToPeakA`; `not_evaluated` otherwise. Passing candidates are now ranked by real total loss instead of area product alone. High-frequency (skin/proximity) loss and thermal rise remain genuinely unimplemented.

**Two ways in:** engineers who already know their inductor's requirements
use `InductorDesignRequest` directly (Mode 2, above). Engineers who instead
know their **Buck converter's** operating point (Vin range, Vout, Iout,
switching frequency, target ripple) can use `POST /topology-design/buck`
(Mode 1) to derive those requirements first — it returns the exact same
`InductorDesignRequest` shape, so Mode 1 is purely an alternate way to
produce the input Mode 2 already accepts; every stage from there on is
identical regardless of which mode produced it. See
[API_REFERENCE.md](API_REFERENCE.md) and [WORKFLOW.md](WORKFLOW.md).
V1 supports Buck only — Boost/Flyback are future work, not started.

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
| **Worked example (Mode 1, Buck converter)** | [WORKED_EXAMPLE_MODE1.md](WORKED_EXAMPLE_MODE1.md) |
| **Worked example (Mode 2, historical) / manual test** | [TESTRESULTSMEAN.md](TESTRESULTSMEAN.md) |
| **Day-to-day run commands** | [WHEN_TO_RUN_PROGRAM.md](WHEN_TO_RUN_PROGRAM.md) |

---
## The Design Workflow (Phase 1)
0. **(Optional) Buck converter requirement derivation** — `POST /topology-design/buck` converts converter-level inputs (Vin range, Vout, Iout, switching frequency, target ripple) into the same `InductorDesignRequest` step 1 below consumes, sized at the worst-case input voltage (`BuckElectricalSolver.cpp`). Skip this step entirely if you already know your inductor's requirements directly.
1. **Material candidates** — every material whose frequency range covers the request (not just the first match)
2. **Area Product (Ap)** — minimum core size needed without overheating, using the named `DesignRules::phase1Default()` ruleset (Ku, Bmax, J) — never hard-coded in the route layer
3. **Core candidates** — every core matching a compatible material; `no_feasible_design` (not a silent oversized fallback) if none meet the Ap requirement
4. **Turns and air-gap design** — iterates turns and gap together until inductance converges within tolerance
5. **Magnetic validation** — inductance, peak flux, saturation margin, winding fit, current density, thermal (six named checks, all failures reported)
6. **Winding design** — round-wire AWG selection, fill factor, current density, DCR from a real geometry-derived mean-length-per-turn (see Status)
7. **Loss evaluation** — DC copper loss when DCR is available; core loss when the material has coefficients and ripple current is supplied; high-frequency loss reported `not_evaluated` (see Status)

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
- ✅ Saturation flux density (`BmaxT`) — real, material-specific data for all 81 materials in the current snapshot; `SaturationValidation`/`PeakFluxValidation` use it automatically instead of the Phase 1 default
- ✅ Core loss (`src/core/losses/CoreLoss.cpp`) — real Steinmetz equation `Pv = k*f^alpha*B^beta` (W/m³) using `data/real_core_loss_coefficients.csv`; `Evaluated` when the material has coefficients at this frequency and the request supplies `rippleCurrentPeakToPeakA` (flux-density swing is never approximated from peak flux), `not_evaluated` otherwise. Temperature correction not yet applied (see [FORMULAS.md](FORMULAS.md) section 9). The web form now has an optional Ripple Current field for this
- ✅ 3-tier recommendation (`src/validation/RecommendationStatus.cpp`) — `Pass`/`ConditionalPass`/`Reject` replaces the old frontend-only "Recommended" sugar. Candidates are ranked by tier first, then real known evaluated loss (copper + core, whichever are `Evaluated`), predicted temperature rise, manufacturability margin, saturation margin, current-density margin, area product, and part number as a deterministic tiebreak — a missing number always ranks as the worst case for that tiebreaker, never the best. `Pass` is currently unreachable in practice (ThermalValidation always flags its result as preliminary) — see [FORMULAS.md](FORMULAS.md) section 12
- ✅ Mode 1: Buck converter requirement derivation (`src/backend/services/BuckElectricalSolver.cpp`, `POST /topology-design/buck`) — derives inductance, peak current, average current, and ripple current from Buck converter operating requirements (Vin range, Vout, Iout, switching frequency, target ripple %), sized at the worst-case Vin. Outputs the same `InductorDesignRequest` Mode 2 accepts directly, so nothing downstream (including RMS-current derivation) is duplicated. Buck only in V1 — Boost/Flyback are not implemented. Web form has a mode toggle for this ("I know my Buck converter requirements")
- ✅ Skin-depth AC-loss risk (`src/core/losses/SkinDepthRisk.cpp`) — real qualitative Low/Moderate/High risk level from strand-radius-vs-skin-depth; never a watts figure (`acLossWattsStatus` permanently `not_evaluated`, no AC-loss watts model exists)
- ✅ Thermal evaluation (`src/core/thermal/ThermalEvaluation.cpp`) — real iterative convergence loop (temp → hot DCR → copper loss → temp rise → repeat); uses a size-aware Rth estimate derived from each candidate's own real core geometry (Newton's law of cooling), falling back to a flat `defaultThermalResistanceCPerW` only when that geometry is unavailable — either way still caps at `PreliminaryThermalEstimate`, never per-core measured/simulated data. `not_evaluated` when DCR geometry is unknown or the loop's real positive-feedback iteration diverges
- ⚠️ Core geometry — `real_cores.csv` has no shape column, so every candidate (including powder toroids) goes through the same discrete-air-gap model; no toroid-specific (distributed-gap) physics yet (see [DATA_FILES.md](DATA_FILES.md))
- ✅ `src/core/magnetics/TurnsCalculation.cpp` is fully implemented (`N = round(sqrt(L/AL))`) — this was previously mis-documented as a stub in several files; it's used as the seed estimate inside `src/core/magnetics/TurnsAndGapDesign.cpp`'s convergence loop
- ✅ Automated tests — `ctest` (C++ unit-conversion + gap/AL formula checks) and `pytest tests/python` (scenario/reference-design checks against `data/test_scenarios.csv` / `data/reference_designs.csv`, with a documented `xfail` for the core-part-number data mismatch — see [DATA_FILES.md](DATA_FILES.md))

---
## Resources
- **Design Reference:** McLyman, *Transformer and Inductor Design Handbook*
- **Data Format:** See [DATA_FILES.md](DATA_FILES.md) for CSV structure
- **Vendor References:** Core geometry sourced from real manufacturer data (Ferroxcube, TDK, Magnetics, Fair-Rite, and others), originally via PyOpenMagnetics/MAS, bundled as a snapshot in `data/real_cores.csv` (see [ARCHITECTURE.md](ARCHITECTURE.md)); one reference design (`i77006`, an IntelliPower part) used for validation