# API Reference

All endpoints are served by the FastAPI app at **`http://127.0.0.1:8000`** (default `uvicorn` host/port — see [WHEN_TO_RUN_PROGRAM.md](WHEN_TO_RUN_PROGRAM.md)). There is no `/api` prefix on any route. All request/response bodies are JSON.

---

## Shared Request Body

All three POST endpoints below accept the same shape (`BuckInput` in `routes/core_selection.py`):

```json
{
"inductanceUH": 250,
"peakCurrentA": 2.0,
"switchingFreqKHz": 100,
"allowableTempRiseC": 40
}
```

Each endpoint only uses the fields it needs internally (e.g. material selection only reads `switchingFreqKHz`), but the same full body is expected on every call.

---

## POST /material-selection

**Purpose:** Choose the best magnetic material for a given switching frequency.

**Request:** the shared body above.

**Response** (`MaterialSelectionResponse`):
```json
{
"materialFamily": "Kool Mu",
"muOpt": 26,
"reason": "Balanced performance 50-250kHz; low core loss; good saturation margin",
"alternatives": "Ferrite|Powder Iron"
}
```
Note: `alternatives` is a single pipe-delimited **string**, not a JSON array — it's passed through as-is from `materials.csv`'s `Alternatives` column.

---

## POST /calculate

**Purpose:** Calculate stored energy and the minimum core area product (Ap).

**Request:** the shared body above.

**Response** (`AreaProductResponse`):
```json
{
"areaProduct": 3.2,
"energy": 0.0005
}
```
`energy` is in **joules** (not millijoules — the frontend multiplies by 1000 for display). `areaProduct` is in cm⁴.

**Note:** window utilization (Ku=0.4), max flux density (Bmax=0.30 T), and current density (J=400 A/cm²) are currently hard-coded inside `build_area_product_input()` in `routes/core_selection.py` — they are not part of the request body and not read from `materials.csv`'s per-material `BmaxT`.

---

## POST /core-selection

**Purpose:** Find a real core from the database that meets the Ap requirement.

**Request:** the shared body above (internally re-runs material selection, then area product, then core selection).

**Response** (`CoreSelectionResponse`):
```json
{
"partNumber": "0077440A7",
"material": "Kool Mu",
"mu": 26,
"al": 59,
"ae": 199,
"wa": 427,
"le": 107
}
```
`ae`, `wa` are in **mm²**; `le` is in **mm** — these match `cores.csv` directly, not cm² as might be assumed.

**No `sort_by` parameter exists.** Core ranking is always by a single internal loss heuristic — cost- and size-based sorting are not implemented.

**No explicit "no core found" error.** If no core in the database meets the Ap requirement (even with the 5% margin), the service falls back to the single largest-Ap core in the database and logs a warning to the console — it does not return an error response.

---

## GET /

Serves `index.html` (the web UI) via `FileResponse`.

## GET /static/*

Serves the frontend's static assets (`app.js`, `styles.css`, etc.) — mounted via FastAPI's `StaticFiles`.

---

## Error Handling

There is no custom error envelope (no `{"status": "error", "message": ...}` pattern). Invalid requests — e.g. a missing or wrong-typed field — return FastAPI's default Pydantic validation error, HTTP 422:

```json
{
"detail": [
{
"loc": ["body", "inductanceUH"],
"msg": "field required",
"type": "value_error.missing"
}
]
}
```

---

## Example: Full Workflow

```bash
# 1. Select material for 100 kHz (full body still required)
curl -X POST http://127.0.0.1:8000/material-selection \
-H "Content-Type: application/json" \
-d '{"inductanceUH": 250, "peakCurrentA": 2.0, "switchingFreqKHz": 100, "allowableTempRiseC": 40}'

# 2. Calculate Ap for the same inputs
curl -X POST http://127.0.0.1:8000/calculate \
-H "Content-Type: application/json" \
-d '{"inductanceUH": 250, "peakCurrentA": 2.0, "switchingFreqKHz": 100, "allowableTempRiseC": 40}'

# 3. Find core matching Ap
curl -X POST http://127.0.0.1:8000/core-selection \
-H "Content-Type: application/json" \
-d '{"inductanceUH": 250, "peakCurrentA": 2.0, "switchingFreqKHz": 100, "allowableTempRiseC": 40}'
```

FastAPI also auto-generates interactive docs at **http://127.0.0.1:8000/docs** — useful for testing without `curl`.

---

## Data Types

| Field | Type | Example |
|---|---|---|
| `inductanceUH` | number | 250 |
| `peakCurrentA` | number | 2.0 |
| `switchingFreqKHz` | number | 100 |
| `allowableTempRiseC` | number | 40 |
| `materialFamily` | string | "Kool Mu" |
| `alternatives` | string (pipe-delimited) | "Ferrite\|Powder Iron" |
| `areaProduct` | number (cm⁴) | 3.2 |
| `energy` | number (joules) | 0.0005 |
| `ae`, `wa` | number (mm²) | 199, 427 |
| `le` | number (mm) | 107 |

---

## Not Yet Implemented (planned endpoints)

Once Stage 4 (Turns/Losses/Thermal) is built, expect additional fields or a new endpoint (e.g. `POST /turns-and-losses`) returning turns count, wire gauge, copper loss, core loss, and predicted temperature rise. None of this exists in the API today.