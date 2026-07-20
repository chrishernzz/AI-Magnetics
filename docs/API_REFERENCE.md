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

**Data-gap fields you will see today:** `losses.coreLossStatus`,
`losses.highFrequencyLossStatus`, and `thermal.status` are `"NotEvaluated"`
for every candidate right now — `CoreLoss.cpp` isn't wired to the real
Steinmetz coefficients yet, skin/proximity loss isn't implemented in Phase
1, and no thermal-resistance model exists yet. `winding.resistanceStatus`
and `losses.copperLossStatus` are `"Evaluated"` whenever a core's `Mlt`
(mean-length-per-turn) is present in `data/real_cores.csv`, which is most
cores today. See [DATA_FILES.md](DATA_FILES.md). These are data gaps, not
bugs — the engine reports them explicitly rather than inventing a number.

---

## Removed: the old single-stage endpoints

Earlier revisions of this API had four separate endpoints
(`/material-selection`, `/calculate`, `/core-selection`,
`/turns-calculation`) that each re-ran every prior stage internally and,
in `/core-selection`'s case, used to silently substitute an oversized core
when nothing actually fit. They were kept for a while as deprecated
wrappers, then removed outright once nothing (including the frontend)
called them anymore — `POST /inductor-design` above is the only endpoint
now, and it does everything those four used to do plus the parts they
never did (turns/gap convergence, magnetic validation, winding design,
loss evaluation). The C++ they were backed by
(`MaterialSelection.cpp`, `CoreSelection.cpp`, `MaterialSelectionService`,
`CoreSelectionService`) was deleted along with them.

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

`/inductor-design` is a complete pipeline, but two result areas are still
reported `not_evaluated` for every candidate today — see
[DATA_FILES.md](DATA_FILES.md):
- **Core loss** (`losses.coreLossStatus`) — real Steinmetz coefficients
  exist in `data/real_core_loss_coefficients.csv` for 17 of the 32
  materials, but `CoreLoss.cpp` isn't wired to consume that file yet.
- **Thermal rise** (`thermal.status`) — no thermal-resistance model or data
  exists yet.

Skin/proximity (high-frequency) loss is not implemented in Phase 1 at all,
regardless of data (`losses.highFrequencyLossStatus`).

DCR / total wire length (`winding.resistanceStatus`) and DC copper loss
(`losses.copperLossStatus`) are no longer blanket gaps — they're
`Evaluated` whenever a core's `Mlt` (mean-length-per-turn) is present in
`data/real_cores.csv`, which is most cores today.