# HTTP API Reference
All endpoints are accessed at `http://localhost:8080/api/` and expect/return JSON.

---
## Material Selection
**Endpoint:** `POST /api/material-select`
**Purpose:** Choose the best magnetic material for a given frequency.
**Request:**
```json
{
  "frequency_khz": 100
}
Response (Success):
{
  "status": "success",
  "material": "Kool Mu",
  "mu_opt": 26,
  "max_frequency_khz": 250000,
  "bmax_t": 1.0,
  "cu_loss_factor": 1.15,
  "alternatives": ["Ferrite 3C90", "Powder Iron"]
}
Response (Error):
{
  "status": "error",
  "message": "Frequency out of supported range"
}


Area Product Calculation
Endpoint: POST /api/area-product
Purpose: Calculate the minimum core size (Ap) needed without overheating.
Request:
{
  "inductance_uh": 250,
  "peak_current_a": 2.0,
  "allowable_temp_rise_c": 40,
  "material": "Kool Mu"
}
Response (Success):
{
  "status": "success",
  "ap_cm4": 3.2,
  "energy_max_mj": 0.5,
  "bmax_t": 1.0,
  "formula_notes": "Ap = 2 * E_max * 10^4 / (Ku * Bmax * J)"
}
Response (Error):
{
  "status": "error",
  "message": "Material not found"
}

Core Selection
Endpoint: POST /api/core-select
Purpose: Find a real core from the database that meets the Ap requirement.
Request:
{
  "ap_cm4": 3.2,
  "material": "Kool Mu",
  "sort_by": "efficiency"
}
sort_by options: "efficiency" (default), "cost", "size"
Response (Success):
{
  "status": "success",
  "selected_core": {
    "part_number": "0077440A7",
    "material": "Kool Mu",
    "ac_cm2": 1.2,
    "wa_cm2": 2.8,
    "le_cm": 8.5,
    "mu_r": 26,
    "al_nh_per_100t": 125
  },
  "candidates": [
    {
      "part_number": "0077440A7",
      "ap_product": 3.36,
      "efficiency_score": 0.92
    }
  ]
}
Response (Error):
{
  "status": "error",
  "message": "No cores meet the Ap requirement"
}

Static Files
Endpoint: GET / (or /index.html)
Purpose: Serve the web UI.
Response: HTML document (index.html)

Error Handling
All error responses include:
"status": "error"
"message": "{description}"
HTTP status code (typically 400 for bad input, 500 for server error)
Common Errors:
"Frequency out of supported range" — Frequency not in materials.csv range
"No cores meet the Ap requirement" — Database doesn't have large enough core
"Material not found" — Requested material doesn't exist in materials.csv



Data Types
Type	Format	Example
frequency_khz	number	100
inductance_uh	number	250
current_a	number	2.0
temp_rise_c	number	40
material	string	"Kool Mu"
sort_by	string enum	"efficiency"
Ap value	number (cm⁴)	3.2

Example: Full Workflow
# 1. Select material for 100 kHz
curl -X POST http://localhost:8080/api/material-select \
  -H "Content-Type: application/json" \
  -d '{"frequency_khz": 100}'
# 2. Calculate Ap for L=250µH, Ipk=2A, ΔT=40°C
curl -X POST http://localhost:8080/api/area-product \
  -H "Content-Type: application/json" \
  -d '{"inductance_uh": 250, "peak_current_a": 2.0, "allowable_temp_rise_c": 40, "material": "Kool Mu"}'
# 3. Find core matching Ap
curl -X POST http://localhost:8080/api/core-select \
  -H "Content-Type: application/json" \
  -d '{"ap_cm4": 3.2, "material": "Kool Mu", "sort_by": "efficiency"}'