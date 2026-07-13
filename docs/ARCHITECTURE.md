---
## **ARCHITECTURE.md** (New)
```markdown
# System Architecture

This document describes the overall design of AIMagnetics: components, responsibilities, and data flow.

---
## High-Level Overview
AIMagnetics is a **three-tier system**:
1. **Frontend (Web UI)** — User enters requirements, displays results
2. **Backend (HTTP Server)** — Routes requests to business logic, returns JSON
3. **Core Engine (C++ Library)** — Implements the magnetic design algorithms

---
## Component Breakdown
### Frontend Layer
**Location:** `src/frontend/`
| File | Purpose |
|------|---------|
| `index.html` | Web page structure; form for requirements input |
| `app.js` | Handles form submission, API calls, result display |
| `styles.css` | Styling for the UI |
**Responsibility:**
- Collect user inputs (inductance, current, frequency, temp rise)
- Send POST requests to backend endpoints
- Parse JSON responses and display results
**Key Endpoints Called:**
- `POST /api/material-select`
- `POST /api/area-product`
- `POST /api/core-select`

---
### Backend Layer
**Location:** `src/backend/`
**Components:**
#### HttpServer
- **File:** `HttpServer.cpp`, `HttpServer.h`
- **Port:** 8080
- **Responsibility:** Listen for HTTP requests, route to controllers, return responses
#### Router
- **File:** `routing/Router.cpp`, `routing/Router.h`
- **Responsibility:** Map URL paths to controller handlers
- **Routes Defined:**
  - `POST /api/material-select` → MaterialSelectionController
  - `POST /api/area-product` → AreaProductController
  - `POST /api/core-select` → CoreSelectionController
  - `GET /` → StaticFileController (serves index.html)
#### Controllers
- **MaterialSelectionController** — Parse request, call MaterialSelectionService, return material + µ_opt
- **AreaProductController** — Parse request, call AreaProductService, return Ap value
- **CoreSelectionController** — Parse request, call CoreSelectionService, return selected core
- **StaticFileController** — Serve static files (HTML, CSS, JS)
#### Services
- **MaterialSelectionService** — Call `MaterialSelection::select()` from core engine
- **AreaProductService** — Call `AreaProduct::calculate()` from core engine
- **CoreSelectionService** — Call `CoreSelection::select()` from core engine

---
### Core Engine Layer
**Location:** `src/core/`, `src/data/`
**Responsibility:** Implement the magnetic design algorithm (4-stage pipeline)
| Module | File | Purpose |
|--------|------|---------|
| MaterialSelection | `MaterialSelection.cpp` | Choose material based on frequency |
| AreaProduct | `AreaProduct.cpp` | Calculate minimum Ap (core size) |
| CoreSelection | `CoreSelection.cpp` | Find best core from database |
| GapDesign | `GapDesign.cpp` | Calculate air gap (if needed) |
| TurnsCalculation | `TurnsCalculation.cpp` | Calculate winding turns for L |
| CopperLoss | `CopperLoss.cpp` | Calculate I²R heating in wire |
| CoreLoss | `CoreLoss.cpp` | Calculate hysteresis + eddy loss |
| HighFrequencyLosses | `HighFrequencyLosses.cpp` | Skin effect and proximity losses |
| CoreDatabase | `data/CoreDatabase.cpp` | Load and query cores.csv |
| Materials | `data/Materials.cpp` | Load and query materials.csv |

---
## Data Flow: From User Input to Result
User enters: L=250µH, Ipk=2A, f=100kHz, ΔT=40°C
↓
Frontend calls: POST /api/material-select with {frequency: 100}
↓
Server routes to MaterialSelectionController
↓
Controller calls MaterialSelectionService.select(100)
↓
Service calls MaterialSelection::select(100) [core engine]
↓
Engine loads materials.csv, finds best match (e.g., Kool Mu)
↓
Service returns: {material: "Kool Mu", mu_opt: 26, ...}
↓
Frontend displays material, enables next step
↓
User clicks "Next" → Frontend calls: POST /api/area-product
with {L: 250, peakCurrent: 2, tempRise: 40, material: "Kool Mu"}
↓
AreaProductService calculates Ap = 2×E_max×10⁴/(Ku×Bmax×J)
↓
Returns: {ap: 3.2, units: "cm⁴"}
↓
Frontend calls: POST /api/core-select
with {ap: 3.2, material: "Kool Mu"}
↓
CoreSelectionService loads cores.csv, finds cores where Ac×Wa >= Ap
↓
Service ranks by efficiency, returns best match
↓
Frontend displays: Part number, core specs, losses

---
## Key Design Patterns
### Service Layer Pattern
- **Controllers** handle HTTP (parsing, routing)
- **Services** handle business logic (calculations, database queries)
- **Core Engine** handles math (algorithms, formulas)
**Why?** Separation of concerns. You can test the service without HTTP, or swap out the HTTP layer later.

### Dependency Flow
Frontend → Backend (HTTP) → Services → Core Engine → Data (CSV)

Each layer is independent. The core engine doesn't know about HTTP.

---
## Database & Data Files
**Location:** `data/`
### cores.csv
- **Purpose:** Core geometry database (vendor datasheets)
- **Loaded by:** `CoreDatabase.cpp`
- **Used by:** `CoreSelection.cpp` (to find cores matching Ap requirement)
- **Fields:** Part number, Ac (cross-section), Wa (window area), Le (path length), material, µ, AL (inductance index)
### materials.csv
- **Purpose:** Material properties (frequency ranges, losses, saturation)
- **Loaded by:** `Materials.cpp`
- **Used by:** `MaterialSelection.cpp`, loss calculators
- **Fields:** Name, µ_opt, frequency range, Bmax, loss factors

---
## Build Configuration
**CMakeLists.txt** defines:
- `magnetics_engine` library (core algorithms + data)
- `magnetics_server` executable (backend + frontend)
- Compiler flags (C++17, platform-specific)
- Linking (ws2_32 on Windows for sockets)

---
## Execution Flow (High Level)
User runs: magnetics_server.exe
main.cpp → HttpServer.start(8080)
Server loads data files (cores.csv, materials.csv)
Server listens on http://localhost:8080
Frontend opens, user fills form
Frontend POSTs to /api/material-select
Server routes → Controller → Service → Core Engine
Engine calculates, returns JSON
Frontend displays result
Repeat for each stage (Ap, Core Selection)


---
## Future Extensibility
To add a new feature (e.g., temperature calculation):
1. **Add to Core Engine:** Create `src/core/Temperature.cpp`
2. **Create Service:** Create `src/backend/services/TemperatureService.cpp`
3. **Create Controller:** Create `src/backend/controllers/TemperatureController.cpp`
4. **Register Route:** Add route in `Router.cpp`
5. **Update Frontend:** Add UI field and API call in `app.js`

---
## Dependencies
- **std::vector, std::map** — C++ standard library (STL)
- **sys/socket, ws2_32** — Platform sockets (for HTTP server)
- **CSV parsing** — Hand-rolled in CoreDatabase.cpp, Materials.cpp
- **No external libraries** — Everything is built from source

---
## Deployment
**Single-Server Model:** Backend + Frontend run on same machine/port.
For cloud deployment, consider:
- Separating frontend (static hosting) from backend (API server)
- Adding HTTPS, authentication
- Scaling the backend across multiple instances