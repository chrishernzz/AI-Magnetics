# AIMagnetics
**Engineer-focused buck inductor sizing tool** following the area-product (Ap) method from McLyman, *Transformer and Inductor Design Handbook*, p.63.

## What It Does
Given your buck converter requirements (inductance, peak current, switching frequency, temperature limits), AIMagnetics recommends an optimal core and material, then calculates turns and losses.
**Input:** L, peak current, switching frequency, allowable temperature rise  
**Output:** Core part number, material, turns count, copper loss, core loss, temperature rise prediction

---
## Quick Links
| I want to... | Read this |
|---|---|
| **Build and run the tool** | [GETTING_STARTED.md](GETTING_STARTED.md) |
| **Understand the design method** | [WORKFLOW.md](WORKFLOW.md) |
| **See the system architecture** | [ARCHITECTURE.md](ARCHITECTURE.md) |
| **Use the web UI** | [FRONTEND_GUIDE.md](FRONTEND_GUIDE.md) |
| **Call the HTTP API** | [API_REFERENCE.md](API_REFERENCE.md) |
| **Add new cores or materials** | [DATA_FILES.md](DATA_FILES.md) |
| **Extend the code** | [DEVELOPMENT.md](DEVELOPMENT.md) |
| **Requirements traceability** | [REQUIREMENTS.md](REQUIREMENTS.md) |

---
## The Design Workflow (4 Stages)
1. **Material Selection** — Choose material based on frequency (Powder Iron, Kool Mu, Ferrite, etc.)
2. **Area Product (Ap)** — Calculate minimum core size needed without overheating
3. **Core Selection** — Find a real core from the database that meets Ap requirement
4. **Validation & Losses** — Calculate turns, copper loss, core loss, verify design
See [WORKFLOW.md](WORKFLOW.md) for the detailed explanation of each stage, formulas, and examples.

---
## Technology Stack
- **Core Engine:** C++17 (magnetics_engine library)
- **Backend Server:** C++ HTTP server (port 8080)
- **Frontend:** Vanilla JavaScript + HTML/CSS
- **Build:** CMake 3.16+
- **Data:** CSV files (cores, materials, reference designs)

---
## Status
- ✅ Buck inductor sizing (non-isolated)
- ✅ Material selection
- ✅ Gap design
- ✅ Loss calculations (copper, core, high-frequency)
- ⚠️ Test scenarios in progress

---
## Resources
- **Design Reference:** McLyman, *Transformer and Inductor Design Handbook*
- **Data Format:** See [DATA_FILES.md](DATA_FILES.md) for CSV structure
- **Vendor References:** Core geometry from supplier datasheets (cores.csv), material properties from datasheets (materials.csv)