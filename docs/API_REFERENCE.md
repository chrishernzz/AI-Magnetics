# API Reference

All endpoints are served by the FastAPI app at **`http://127.0.0.1:8000`** (default `uvicorn` host/port — see [WHEN_TO_RUN_PROGRAM.md](WHEN_TO_RUN_PROGRAM.md)). There is no `/api` prefix on any route. All request/response bodies are JSON.

---

## POST /inductor-design (canonical Phase 1 endpoint)

**Purpose:** Run the full Phase 1 pipeline once (materials → area product →
cores → turns/gap → magnetic validation → winding → losses → thermal →
ranking) and return one explainable result.

**Request** (`InductorDesignRequest`, defined once in `routes/inductor_design.py`
— the renamed, extended successor to the old shared `BuckInput`; every field
is a direct inductor specification with no topology knowledge):

```json
{
  "inductanceUH": 250,
  "peakCurrentA": 5.0,
  "rmsCurrentA": 3.5,
  "switchingFreqKHz": 100,
  "ambientTemperatureC": 25,
  "allowableTempRiseC": 40,
  "inductanceTolerancePercent": 10
}
```

`rmsCurrentA` may be omitted if `averageCurrentA` and
`rippleCurrentPeakToPeakA` are both supplied instead — RMS current is then
derived assuming a **triangular ripple waveform**
(`Irms = sqrt(Iavg^2 + ripple^2/12)`), and `withinTolerance`/validation
explanations note this assumption was used. Peak current is never used to
infer RMS current. If neither is supplied, the request fails with HTTP 422.

`inductanceTolerancePercent` defaults to `DesignRules.defaultInductanceTolerancePercent`
(10%) if omitted. Optional fields not yet consumed by every stage:
`maximumDcrMilliOhm`, `maximumWidthMm`, `maximumHeightMm`, `maximumLengthMm`,
`preferredMaterialFamily`, `preferredCoreGeometry`.

**Response** (`DesignRecommendation`, serialized from the C++ struct of the same name):

```json
{
  "status": "ok",
  "message": "1 candidate(s) passed every check; 2 rejected.",
  "candidates": [
    {
      "material": { "materialFamily": "3C90", "muOpt": 2249.28, "hasBmaxData": false, "missingDataWarnings": ["..."], "..." : "..." },
      "core": { "partNumber": "E100/60/28-3C90", "areaProductCm4": 15.7, "meetsAreaProduct": true, "..." : "..." },
      "turnsAndGap": { "turns": 42, "gapMm": 1.15, "calculatedInductanceUH": 249.8, "inductanceErrorPercent": -0.08, "withinTolerance": true, "converged": true },
      "validations": [ { "checkName": "InductanceValidation", "passed": true, "..." : "..." }, "... five more ..." ],
      "winding": { "wireDescription": "AWG18 single strand", "fillFactor": 0.11, "fitsWindow": true, "resistanceStatus": "NotEvaluated", "missingData": ["core '...' has no mean-length-per-turn data..."] },
      "losses": { "copperLossStatus": "NotEvaluated", "coreLossStatus": "NotEvaluated", "highFrequencyLossStatus": "NotEvaluated", "missingData": ["..."] },
      "thermal": { "status": "NotEvaluated", "missingDataExplanation": "no thermal-resistance model or data is available in Phase 1" },
      "passed": true,
      "rejectionReasons": []
    }
  ],
  "rejectedCandidates": ["... same shape, passed=false, rejectionReasons populated ..."],
  "activeRules": { "windowUtilization": 0.4, "allowableCurrentDensityAperCm2": 400.0, "defaultFluxDensityLimitT": 0.30, "minimumSaturationMarginPercent": 10.0, "maximumFillFactor": 0.6, "defaultInductanceTolerancePercent": 10.0, "minimumSingleStrandAwg": 18 },
  "requiredAreaProductCm4": 0.0,
  "largestAvailableAreaProductCm4": 0.0
}
```

`activeRules` is always returned — this is the entire `DesignRules::phase1Default()`
ruleset, so no assumption (Ku, Bmax, J, tolerances) is ever hidden in the
route layer (spec section 7).

When nothing is feasible, `status` is `"no_feasible_design"` instead of
`"ok"`, `candidates` is empty, and `message` explains why — for an
area-product shortfall specifically, `requiredAreaProductCm4` and
`largestAvailableAreaProductCm4` are populated so the gap is visible, not
just a console warning:

```json
{
  "status": "no_feasible_design",
  "message": "No core met the area-product requirement.",
  "candidates": [],
  "rejectedCandidates": [],
  "requiredAreaProductCm4": 8.4,
  "largestAvailableAreaProductCm4": 6.9
}
```

**Data-gap fields you will see today:** `winding.resistanceStatus`,
`losses.copperLossStatus`, `losses.coreLossStatus`,
`losses.highFrequencyLossStatus`, and `thermal.status` are `"NotEvaluated"`
for every candidate right now — `data/real_cores.csv` has no
mean-length-per-turn column, `data/real_materials.csv`'s `BmaxT`/
`CuLossFactor` are 0.0 for every material, and no thermal-resistance model
exists yet. See [DATA_FILES.md](DATA_FILES.md). These are data gaps, not
bugs — the engine reports them explicitly rather than inventing a number.

---

## Legacy endpoints (deprecated)

The four endpoints below predate the Phase 1 engine and are kept only for
backward compatibility (`deprecated=True` in FastAPI, so they're flagged in
`/docs`). None of them perform turns/gap convergence, magnetic validation,
winding design, or loss evaluation — use `POST /inductor-design` above.
They share the same `InductorDesignRequest` model (so `ambientTemperatureC`
etc. are required in the request body even though these routes' internal
logic doesn't read them), but internally still run the old single-pick
C++ path unchanged.

### Shared Request Body (legacy)

```json
{
"inductanceUH": 250,
"peakCurrentA": 2.0,
"switchingFreqKHz": 100,
"ambientTemperatureC": 25,
"allowableTempRiseC": 40
}
```

Each endpoint only uses the fields it needs internally (e.g. material selection only reads `switchingFreqKHz`), but the same full body is expected on every call.

---

## POST /material-selection *(deprecated — see above)*

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
Note: `alternatives` is a single pipe-delimited **string**, not a JSON array — it's passed through as-is from the material record's `Alternatives` field (currently always `"None"`; the bundled real-data snapshot doesn't populate this yet, see `python/services/magnetics_data.py`).

---

## POST /calculate *(deprecated — see above)*

**Purpose:** Calculate stored energy and the minimum core area product (Ap).

**Request:** the legacy shared body above.

**Response** (`AreaProductResponse`):
```json
{
"areaProduct": 3.2,
"energy": 0.0005
}
```
`energy` is in **joules** (not millijoules — the frontend multiplies by 1000 for display). `areaProduct` is in cm⁴.

**Note:** window utilization (Ku), max flux density (Bmax), and current density (J) are sourced from `magnetics_cpp.design_rules_phase1_default()` (the same named ruleset `/inductor-design` uses) inside `build_area_product_input()` in `routes/core_selection.py` — no constant is hard-coded in the Python route (spec section 7). They are still not part of the request body and not read from each material's `BmaxT` field (0.0/unpopulated in the current real-data snapshot).

---

## POST /core-selection *(deprecated — see above)*

**Purpose:** Find a real core from the database that meets the Ap requirement (legacy single pick).

**Request:** the legacy shared body above (internally re-runs material selection, then area product, then core selection).

**Response** (`CoreSelectionResponse`):
```json
{
"partNumber": "E100/60/28-3C90",
"material": "3C90",
"mu": 2249.28,
"al": 7584.86,
"ae": 735.05,
"wa": 2138.7,
"le": 273.92
}
```
`ae`, `wa` are in **mm²**; `le` is in **mm** — these match the real core data's `Ae`/`Wa`/`Le` fields directly (see `python/services/magnetics_data.py`), not cm² as might be assumed.

**No `sort_by` parameter exists.** Core ranking is always by a single internal loss heuristic — cost- and size-based sorting are not implemented.

**No silent oversized fallback.** If no core in the database meets the Ap requirement (even with the 5% margin), this legacy endpoint returns `{"partNumber": "No compatible core found", "material": "Unknown", ...}` rather than silently substituting the largest available core. (This changed in Phase 1 — it previously did fall back silently.) `POST /inductor-design` reports the same situation as `status: "no_feasible_design"` with the required/largest-available area product.

---

## POST /turns-calculation *(deprecated — see above)*

**Purpose:** Legacy AL-only turns formula (`N = round(sqrt(L/AL))` against the *ungapped* catalog AL) — no gap iteration, unlike `/inductor-design`'s `TurnsAndGapDesign`.

**Request:** the legacy shared body above (internally re-runs core selection, which re-runs material selection).

**Response** (`TurnsCalculationResponse`): `{"turns": 42, "inductanceUH": 250, "al": 7584.86}`

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
# The canonical Phase 1 call - runs the whole pipeline once
curl -X POST http://127.0.0.1:8000/inductor-design \
-H "Content-Type: application/json" \
-d '{"inductanceUH": 250, "peakCurrentA": 5.0, "rmsCurrentA": 3.5, "switchingFreqKHz": 100, "ambientTemperatureC": 25, "allowableTempRiseC": 40}'
```

```bash
# Legacy single-stage calls (deprecated)
curl -X POST http://127.0.0.1:8000/material-selection \
-H "Content-Type: application/json" \
-d '{"inductanceUH": 250, "peakCurrentA": 2.0, "switchingFreqKHz": 100, "ambientTemperatureC": 25, "allowableTempRiseC": 40}'

curl -X POST http://127.0.0.1:8000/calculate \
-H "Content-Type: application/json" \
-d '{"inductanceUH": 250, "peakCurrentA": 2.0, "switchingFreqKHz": 100, "ambientTemperatureC": 25, "allowableTempRiseC": 40}'

curl -X POST http://127.0.0.1:8000/core-selection \
-H "Content-Type: application/json" \
-d '{"inductanceUH": 250, "peakCurrentA": 2.0, "switchingFreqKHz": 100, "ambientTemperatureC": 25, "allowableTempRiseC": 40}'
```

FastAPI also auto-generates interactive docs at **http://127.0.0.1:8000/docs** — useful for testing without `curl`.

---

## Data Types

| Field | Type | Example |
|---|---|---|
| `inductanceUH` | number | 250 |
| `peakCurrentA` | number | 5.0 |
| `rmsCurrentA` | number, optional (see derivation rule) | 3.5 |
| `switchingFreqKHz` | number | 100 |
| `ambientTemperatureC` | number | 25 |
| `allowableTempRiseC` | number | 40 |
| `inductanceTolerancePercent` | number, optional | 10 |
| `status` | string | "ok" \| "no_feasible_design" |
| `materialFamily` | string | "3C90" |
| `alternatives` | string (pipe-delimited) | "None" |
| `areaProductCm4` | number (cm⁴) | 3.2 |
| `aeMm2`, `waMm2` | number (mm²) | 735.05, 2138.7 |
| `leMm`, `gapMm` | number (mm) | 273.92, 1.15 |
| `*Status` fields (`resistanceStatus`, `copperLossStatus`, etc.) | string | "Evaluated" \| "NotEvaluated" \| "Rejected" |

---

## Known Phase 1 Data Gaps

`/inductor-design` is a complete pipeline, but three result areas are
reported `not_evaluated` for every candidate today because the bundled
data snapshot doesn't carry the inputs they need — see
[DATA_FILES.md](DATA_FILES.md):
- **DCR / total wire length** (`winding.resistanceStatus`) — no
  mean-length-per-turn column in `data/real_cores.csv`
- **Core loss** (`losses.coreLossStatus`) — `CuLossFactor` is 0.0 for
  every material in `data/real_materials.csv`
- **Thermal rise** (`thermal.status`) — no thermal-resistance model or data
  exists yet

DC copper loss (`losses.copperLossStatus`) depends on DCR, so it is also
`not_evaluated` until the mean-length-per-turn data gap above is closed.
Skin/proximity (high-frequency) loss is not implemented in Phase 1
regardless of data (`losses.highFrequencyLossStatus`).