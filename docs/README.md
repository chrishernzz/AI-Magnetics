# AIMagnetics
**Engineer-focused power inductor sizing tool** — topology-agnostic (buck, boost, flyback output inductor, etc.), following the area-product (Ap) method from McLyman, *Transformer and Inductor Design Handbook*, p.63.

## What It Does
Given your inductor requirements (inductance, peak current, switching frequency, temperature limits), AIMagnetics recommends an optimal core and material. Turns and loss calculation are planned but not yet implemented — see Status below.

**Input:** L, peak current, switching frequency, allowable temperature rise
**Output today:** Recommended material, recommended core (part number + geometry), stored energy, area product (Ap)
**Output planned:** Turns count, wire gauge, copper loss, core loss, temperature rise prediction

---
## Quick Links

| I want to... | Read this |
|---|---|
| **Build and run the tool** | [GETTING_STARTED.md](GETTING_STARTED.md) |
| **Understand the design method** | [WORKFLOW.md](WORKFLOW.md) |
| **See the system architecture** | [ARCHITECTURE.md](ARCHITECTURE.md) |
| **Use the web UI** | [FRONTEND_GUIDE.md](FRONTEND_GUIDE.md) |
| **Call the API** | [API_REFERENCE.md](API_REFERENCE.md) |
| **Add new cores or materials** | [DATA_FILES.md](DATA_FILES.md) |
| **Requirements traceability** | [REQUIREMENTS.md](REQUIREMENTS.md) |
| **Worked example / manual test** | [TESTRESULTSMEAN.md](TESTRESULTSMEAN.md) |
| **Day-to-day run commands** | [WHEN_TO_RUN_PROGRAM.md](WHEN_TO_RUN_PROGRAM.md) |

*(`DEVELOPMENT.md` is linked from nowhere else and doesn't exist yet — either write it or remove this line.)*

---
## The Design Workflow (4 Stages)
1. **Material Selection** — Choose material based on frequency (Powder Iron, Kool Mu, Ferrite, etc.)
2. **Area Product (Ap)** — Calculate minimum core size needed without overheating
3. **Core Selection** — Find a real core from the database that meets the Ap requirement
4. **Validation & Losses** — Turns, copper loss, core loss, temperature rise — **not yet implemented**

See [WORKFLOW.md](WORKFLOW.md) for formulas, worked examples, and the current status of each stage.

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
- ✅ Material selection (frequency-range match against `materials.csv`)
- ✅ Area product calculation
- ✅ Core selection (Ap filter + material filter + single-metric loss ranking)
- ❌ Gap design — stubbed, not implemented
- ❌ Turns calculation — stubbed, not implemented
- ❌ Copper loss, core loss, high-frequency loss — stubbed, not implemented
- ❌ Validation layer (flux density check, fill factor check) — header only, not compiled
- ⚠️ Test scenarios — 7 defined in `data/test_scenarios.csv`, checked manually; no automated runner yet

---
## Resources
- **Design Reference:** McLyman, *Transformer and Inductor Design Handbook*
- **Data Format:** See [DATA_FILES.md](DATA_FILES.md) for CSV structure
- **Vendor References:** Core geometry currently sourced from Magnetics Inc. datasheets (`cores.csv`); one reference design (`i77006`, an IntelliPower part) used for validation