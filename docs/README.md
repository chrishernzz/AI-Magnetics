# AIMagnetics
**Engineer-focused power inductor sizing tool** — topology-agnostic (buck, boost, flyback output inductor, etc.), following the area-product (Ap) method from McLyman, *Transformer and Inductor Design Handbook*, p.63.

## What It Does
Given your inductor requirements (inductance, peak/RMS current, switching frequency, temperature limits), AIMagnetics runs the full design pipeline: material candidates → area product → core candidates → turns + gap design (machined air gap for ferrite/E-cores, distributed-gap DC-bias roll-off for powder toroids/E-cores) → magnetic validation (inductance, peak flux, saturation, winding fit, current density, bundle fit, thermal) → round-wire winding design → copper loss → core loss → thermal (iterative convergence loop) → ranking. Every candidate's pass/fail is explainable, with rejection reasons and missing-data warnings rather than a single silent pick — see Status below for what's fully evaluated vs. `not_evaluated` pending more data.

**Input:** `InductorDesignRequest` — inductance, peak current, RMS current (or derivable from average current + ripple), switching frequency, ambient temperature, allowable temperature rise, inductance tolerance (see [API_REFERENCE.md](API_REFERENCE.md))
**Output:** `POST /inductor-design` returns a `DesignRecommendation` — passing and rejected candidates, each with material, core, turns/gap, validation results, winding design, and losses; or `status: "no_feasible_design"` with the reason, never a silent oversized fallback
**Known gaps:** High-frequency (skin/proximity) AC-loss watts are not modeled — `SkinDepthRisk.cpp` reports a qualitative Low/Moderate/High risk level only, never a watts figure. Core loss requires both a material with real Steinmetz coefficients and a supplied ripple current; otherwise it reports `not_evaluated` rather than a guess.

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
| **Day-to-day run commands** | [WHEN_TO_RUN_PROGRAM.md](WHEN_TO_RUN_PROGRAM.md) |

---
## The Design Workflow

0. **(Optional) Buck converter requirement derivation** — `POST /topology-design/buck` converts converter-level inputs (Vin range, Vout, Iout, switching frequency, target ripple) into the same `InductorDesignRequest` step 1 below consumes, sized at the worst-case input voltage (`BuckElectricalSolver.cpp`). Skip this step entirely if you already know your inductor's requirements directly.
1. **Material candidates** — every material whose frequency range covers the request (not just the first match)
2. **Area Product (Ap)** — minimum core size needed without overheating, using the named `DesignRules::phase1Default()` ruleset (Ku=0.4, Bmax=0.30T, J=986.76 A/cm² — Roger's stated 200 circular-mils-per-amp rule) — never hard-coded in the route layer
3. **Core candidates** — every core matching a compatible material; `no_feasible_design` (not a silent oversized fallback) if none meet the Ap requirement
4. **Turns and gap design** — machined-gap ferrite/powder-E-core parts iterate turns and gap together until inductance converges within tolerance; distributed-gap powder toroids/E-cores solve turns once from the zero-bias catalog AL and hold them fixed, applying a real, manufacturer-published DC-bias permeability roll-off curve to get the inductance actually delivered at your operating current
5. **Magnetic validation** — inductance, peak flux, saturation margin, winding fit, current density, bundle fit, thermal (eight named checks, all failures reported)
6. **Winding design** — round-wire AWG selection, fill factor, current density, DCR from a real geometry-derived mean-length-per-turn
7. **Loss evaluation** — copper loss when DCR is available; core loss when the material has Steinmetz coefficients and ripple current is supplied; high-frequency loss stays `not_evaluated` (no watts model exists)
8. **Thermal evaluation** — iterative convergence loop (temperature → hot DCR → copper loss → temperature rise → repeat) using a 3-tier thermal-resistance estimate: a real, manufacturer-published wound-coil surface area when transcribed for that part, an Ae×Le compact-solid shape-factor estimate when it isn't, or a flat default when even core geometry is unavailable
9. **Ranking** — candidates are ordered by recommendation tier, then DC-bias permeability retention, then known evaluated loss, predicted temperature rise, manufacturability margin, saturation margin, current-density margin, area product, and part number as a final deterministic tiebreak

See [WORKFLOW.md](WORKFLOW.md) for formulas and the current status of each stage.

---
## Technology Stack
- **Core Engine:** C++17 (`magnetics_engine`, `magnetics_services` — CMake static libraries)
- **Python Bindings:** pybind11 (`magnetics_cpp` module, built by CMake)
- **Backend:** FastAPI (Python), run via `uvicorn`
- **Frontend:** Vanilla JavaScript + HTML/CSS
- **Build:** CMake 3.16+
- **Data:** CSV files (cores, materials, DC-bias curves, core-loss coefficients, reference designs, test scenarios)

---

## Status
- ✅ Material candidate evaluation (`src/core/sizing/MaterialEvaluation.cpp` — `findSuitableMaterials()`)
- ✅ Area product calculation, sourced from the named `DesignRules::phase1Default()` ruleset
- ✅ Core candidate evaluation (`src/core/sizing/CoreEvaluation.cpp` — `findSuitableCores()`); no silent oversized fallback — returns `no_feasible_design` with the required/largest-available area product instead
- ✅ Gap design and turns/gap convergence (`src/core/magnetics/TurnsAndGapDesign.cpp`) — dispatches per candidate to `solveMachinedGapCore()` (ferrite/powder E-cores with a discrete air gap, iterated) or `solveDistributedGapCore()` (powder toroids/E-cores, turns fixed at the zero-bias solve, DC-bias roll-off applied against the fixed turns count)
- ✅ DC-bias permeability roll-off (`src/core/magnetics/PermeabilityRolloff.cpp`, `data/dc_bias_curves.csv`) — real, manufacturer-published per-material curves; `TurnsAndGapResult::percentInitialPermeabilityAtOperatingCurrent` reports how much of the catalog (0-bias) permeability survives at the real operating current, and this is now a ranking criterion (see below)
- ✅ Magnetic validation (`DesignValidation.cpp`) — CurrentConsistencyValidation, InductanceValidation, PeakFluxValidation, SaturationValidation, WindingFitValidation, CurrentDensityValidation, BundleFitValidation, ThermalValidation
- ✅ Winding design (`src/core/winding/WindingDesign.cpp`) — AWG wire selection, fill factor, current density
- ✅ Copper loss (`src/core/losses/CopperLoss.cpp`), called from `src/core/losses/LossEvaluation.cpp` — real DCR from `CoreCandidate.mltMm` (see [DATA_FILES.md](DATA_FILES.md))
- ✅ Saturation flux density (`BmaxT`) — real, material-specific data for every material in the current snapshot
- ✅ Core loss (`src/core/losses/CoreLoss.cpp`) — real Steinmetz equation `Pv = k*f^alpha*B^beta` (W/m³) using `data/real_core_loss_coefficients.csv`; `Evaluated` when the material has coefficients at this frequency and the request supplies `rippleCurrentPeakToPeakA`, `not_evaluated` otherwise
- ✅ Thermal evaluation (`src/core/thermal/ThermalEvaluation.cpp`) — real iterative convergence loop, 3-tier thermal-resistance estimate: real manufacturer-published wound-coil surface area (`CoreCandidate::surfaceAreaWoundMm2`, transcribed for 423 of 755 cores as of this snapshot) → Ae×Le compact-solid shape-factor estimate → flat `defaultThermalResistanceCPerW`. `ThermalEvaluationResult::thermalResistanceUsesRealSurfaceArea` reports which tier was used. Still caps at `PreliminaryThermalEstimate`, never per-core measured/simulated data; `not_evaluated` when DCR geometry is unknown or the loop's positive-feedback iteration diverges
- ✅ 3-tier recommendation (`src/validation/RecommendationStatus.cpp`) — `Pass`/`ConditionalPass`/`Reject`. Candidates are ranked by tier first, then DC-bias permeability retention (higher is better — a distributed-gap candidate that barely holds its target inductance under real current no longer outranks one that retains it), then known evaluated loss, predicted temperature rise, manufacturability margin, saturation margin, current-density margin, area product, and part number as a deterministic tiebreak. `Pass` is currently unreachable in practice (`ThermalValidation` always flags its result as preliminary) — see [FORMULAS.md](FORMULAS.md)
- ✅ Mode 1: Buck converter requirement derivation (`src/backend/services/BuckElectricalSolver.cpp`, `POST /topology-design/buck`) — derives inductance, peak current, average current, and ripple current from Buck converter operating requirements, sized at the worst-case Vin. Outputs the same `InductorDesignRequest` Mode 2 accepts directly. Buck only in V1
- ✅ Skin-depth AC-loss risk (`src/core/losses/SkinDepthRisk.cpp`) — qualitative Low/Moderate/High risk level from strand-radius-vs-skin-depth; never a watts figure
- ✅ Automated tests — `ctest` (C++ unit/formula/thermal/ranking checks) and `pytest tests/python` (scenario/reference-design/golden checks against the real loaded database)

---
## Resources
- **Design Reference:** McLyman, *Transformer and Inductor Design Handbook*
- **Data Format:** See [DATA_FILES.md](DATA_FILES.md) for CSV structure
- **Vendor References:** All core, material, DC-bias-curve, core-loss-coefficient, and surface-area data is sourced from Magnetics Inc.'s own published datasheets (mag-inc.com), transcribed by hand part-by-part into `data/*.csv` (see [ARCHITECTURE.md](ARCHITECTURE.md)) — no third-party magnetics library involved
