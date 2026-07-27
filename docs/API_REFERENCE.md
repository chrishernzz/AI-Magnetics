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
      "material": { "materialFamily": "3C90", "muOpt": 2249.28, "hasBmaxData": true, "source": { "confidence": "Manufacturer", "..." : "..." }, "missingDataWarnings": ["..."], "..." : "..." },
      "core": { "partNumber": "E100/60/28-3C90", "vendor": "Ferroxcube", "areaProductCm4": 15.7, "meetsAreaProduct": true, "source": { "..." : "..." }, "..." : "..." },
      "turnsAndGap": { "turns": 42, "gapMm": 1.15, "calculatedInductanceUH": 249.8, "inductanceErrorPercent": -0.08, "withinTolerance": true, "converged": true, "gapMinMm": 1.035, "gapMaxMm": 1.265, "inductanceWithinToleranceAcrossGapRange": true, "smallGapWarning": false },
      "validations": [ { "checkName": "InductanceValidation", "passed": true, "isPreliminaryEstimate": false, "..." : "..." }, "... five more (including ThermalValidation, isPreliminaryEstimate: true when Evaluated) ..." ],
      "winding": { "wireDescription": "AWG18 single strand", "fillFactor": 0.11, "fitsWindow": true, "physicalWindowFillFactor": 0.21, "fitsPhysicalWindow": true, "resistanceStatus": "Evaluated", "coldDcrOhmsAt20C": 0.045, "estimatedHotDcrOhms": 0.052, "physicalDescription": "AWG18 magnet wire (bare 1.02 mm, insulated ~1.07 mm), single strand", "missingData": [] },
      "losses": { "copperLossStatus": "Evaluated", "copperLossW": 0.55, "coreLossStatus": "NotEvaluated", "missingData": ["..."] },
      "thermal": { "status": "PreliminaryThermalEstimate", "convergedWindingTempC": 38.2, "predictedTempRiseC": 13.2, "converged": true, "iterationsUsed": 3, "thermalResistanceCPerWUsed": 15.0 },
      "acLossRisk": { "riskLevel": "Low", "reason": "evaluated: single-strand skin effect...", "acLossWattsStatus": "NotEvaluated" },
      "fluxLimits": { "absoluteSaturationT": 0.47, "recommendedOperatingT": 0.3995, "temperatureAdjustedStatus": "NotEvaluated", "coreLossLimitedStatus": "NotEvaluated" },
      "recommendation": { "tier": "PreliminaryCandidate", "checksEvaluatedCount": 6, "checksPassedCount": 6, "checksNotEvaluatedCount": 0, "explanation": "passed every check that ran, but at least one check rests on a Phase 1 default assumption, not measured data; " },
      "lossSummary": { "knownEvaluatedLossW": 0.55, "isCompleteTotal": false, "label": "Known Evaluated Loss (copper - partial coverage, AC/skin-effect loss not modeled)" },
      "manufacturabilityMarginPercent": 71.4,
      "rankingExplanation": "[PreliminaryCandidate] passed every check that ran, but ...",
      "passed": true,
      "rejectionReasons": []
    }
  ],
  "rejectedCandidates": ["... same shape, passed=false, rejectionReasons populated, recommendation.tier=Rejected ..."],
  "activeRules": { "windowUtilization": 0.4, "allowableCurrentDensityAperCm2": 400.0, "defaultFluxDensityLimitT": 0.30, "minimumSaturationMarginPercent": 10.0, "maximumFillFactor": 0.6, "defaultInductanceTolerancePercent": 10.0, "minimumSingleStrandAwg": 18, "gapTolerancePercent": 10.0, "defaultThermalResistanceCPerW": 15.0, "...": "30 fields total - see DesignRules.h" },
  "requiredAreaProductCm4": 0.0,
  "largestAvailableAreaProductCm4": 0.0,
  "versions": { "calculationEngineVersion": "1.1.0", "designRulesVersion": "1.1.0", "coreDatabaseVersion": "60 rows, sha256:...", "materialDatabaseVersion": "32 rows, sha256:..." }
}
```

`activeRules` is always returned — this is the entire `DesignRules::phase1Default()`
ruleset (30 fields as of this pass), so no assumption (Ku, Bmax, J,
tolerances, gap/thermal/skin-depth heuristics) is ever hidden in the route
layer (spec section 7). `versions` is a real, reproducible SHA-256 content
hash of the loaded CSV bytes, computed once at FastAPI startup — not an
invented upstream semver.

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

**Data-gap fields you may still see today:** `thermal.status` is
`"PreliminaryThermalEstimate"` at best, never a "fully evaluated" value —
`ThermalStatus` has no such value at all, since the loop always runs on
`DesignRules.defaultThermalResistanceCPerW` (a Phase 1 default, never
per-core measured data). `acLossRisk.acLossWattsStatus` is permanently
`"NotEvaluated"` — the skin-depth heuristic produces a risk *level*, never
a watts figure. `winding.resistanceStatus` and `losses.copperLossStatus`
are `"Evaluated"` for most candidates (real, geometry-derived
mean-length-per-turn data unlocked real DCR/copper loss), `"NotEvaluated"`
only for the smaller subset of cores whose upstream geometry doesn't
support that estimate. `losses.coreLossStatus` is `"Evaluated"` when the
material has real Steinmetz coefficients for this frequency AND the
request supplied `rippleCurrentPeakToPeakA` (needed to compute
flux-density swing — never approximated from peak flux); `"NotEvaluated"`
otherwise. See [DATA_FILES.md](DATA_FILES.md). These are real data gaps,
not silent bugs — the engine reports them explicitly rather than
inventing a number, and every candidate's `recommendation.tier` reflects
them (a `not_evaluated`/preliminary check anywhere caps a candidate at
`PreliminaryCandidate`, never `Phase1Recommended` — see FORMULAS.md
section 12 for why `Phase1Recommended` is currently unreachable in
practice).

---

## POST /topology-design/buck (Mode 1: derive requirements from a Buck converter)

**Purpose:** For engineers who know their converter's operating point but
not their inductor's requirements yet. Converts Buck converter inputs into
the same fields `/inductor-design` accepts directly — nothing downstream
of this call knows or cares whether a request came from here or from
direct entry (Mode 2). This does **not** run the design pipeline itself;
call `/inductor-design` with the response to get a `DesignRecommendation`.

V1 supports Buck only (`BuckElectricalSolver.h`) — Boost/Flyback would be
one more solver and one more route (`/topology-design/boost`, etc.), not a
change to this one.

**Request** (`BuckTopologyInput`, defined in `routes/topology_design.py`):

```json
{
  "vinMinV": 36,
  "vinMaxV": 60,
  "voutV": 12,
  "ioutA": 40,
  "switchingFreqKHz": 500,
  "rippleCurrentPercent": 20,
  "ambientTemperatureC": 25,
  "allowableTempRiseC": 40
}
```

`rippleCurrentPercent` is the target inductor peak-to-peak ripple current
as a percentage of `ioutA`. `inductanceTolerancePercent` is optional, same
default behavior as `/inductor-design`. Inductance and ripple current are
sized at `vinMaxV` — a buck converter's inductor ripple current is worst
(highest) at the top of the input voltage range, so sizing there keeps
ripple at or under the target across the whole `vinMinV`..`vinMaxV` range.
This single-worst-case-point approach is a documented V1 simplification;
it does not evaluate `vinMinV` separately.

**Response** (mirrors `InductorDesignRequest`'s own field names):

```json
{
  "inductanceUH": 2.4,
  "peakCurrentA": 44.0,
  "switchingFreqKHz": 500.0,
  "ambientTemperatureC": 25.0,
  "allowableTempRiseC": 40.0,
  "inductanceTolerancePercent": null,
  "averageCurrentA": 40.0,
  "rippleCurrentPeakToPeakA": 8.0
}
```

`rmsCurrentA` is deliberately absent (`null` if included at all) —
`averageCurrentA` and `rippleCurrentPeakToPeakA` are the real derived
values, and `RequirementDerivationService` derives RMS current from them
downstream using the same triangular-ripple formula `/inductor-design`
already uses for direct entry. There is exactly one RMS derivation in the
codebase regardless of which endpoint produced the request. Feed this
response directly into `/inductor-design` to run the actual pipeline:

```bash
DERIVED=$(curl -s -X POST http://127.0.0.1:8000/topology-design/buck \
  -H "Content-Type: application/json" \
  -d '{"vinMinV":36,"vinMaxV":60,"voutV":12,"ioutA":40,"switchingFreqKHz":500,"rippleCurrentPercent":20,"ambientTemperatureC":25,"allowableTempRiseC":40}')

curl -X POST http://127.0.0.1:8000/inductor-design \
  -H "Content-Type: application/json" \
  -d "$DERIVED"
```

Returns HTTP 422 if the inputs aren't physically valid for a buck
converter (e.g. `voutV >= vinMaxV`) — `BuckElectricalSolver` validates
before computing rather than returning a negative or NaN inductance.

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
| `vinMinV`, `vinMaxV`, `voutV` | number (V, `/topology-design/buck` only) | 36, 60, 12 |
| `ioutA` | number (A, `/topology-design/buck` only) | 40 |
| `rippleCurrentPercent` | number (% of `ioutA`, `/topology-design/buck` only) | 20 |
| `status` | string | "ok" \| "no_feasible_design" |
| `materialFamily` | string | "3C90" |
| `alternatives` | string (pipe-delimited) | "None" |
| `areaProductCm4` | number (cm⁴) | 3.2 |
| `aeMm2`, `waMm2` | number (mm²) | 735.05, 2138.7 |
| `leMm`, `gapMm` | number (mm) | 273.92, 1.15 |
| `*Status` fields (`resistanceStatus`, `copperLossStatus`, etc.) | string | "Evaluated" \| "NotEvaluated" \| "Rejected" |

---

## Known Phase 1 Data Gaps

`/inductor-design` is a complete pipeline; two result areas are always
capped for lack of real measured/simulated data, by construction, not by
oversight — see [DATA_FILES.md](DATA_FILES.md) and FORMULAS.md sections 10-11:
- **Thermal rise** (`thermal.status`) — a real iterative loop runs, but
  `defaultThermalResistanceCPerW` is always a Phase 1 default constant, so
  the result caps at `"PreliminaryThermalEstimate"`, never a fully-evaluated
  value. Reports `"NotEvaluated"` instead when winding DCR geometry is
  unknown, or when the loop's positive-feedback iteration genuinely
  diverges (a real possibility for high-current, low-DCR designs).
- **AC-loss watts** (`acLossRisk.acLossWattsStatus`) — a real qualitative
  skin-depth risk level (`Low`/`Moderate`/`High`) is computed, but no
  AC-loss watts model exists, so this status is permanently `"NotEvaluated"`.

The rest are conditionally real, never fabricated:
- **Core loss** (`losses.coreLossStatus`) — `Evaluated` when the material
  has real Steinmetz coefficients for the request's frequency AND the
  request supplied `rippleCurrentPeakToPeakA`; `not_evaluated` otherwise
  (25 of 81 materials have coefficients; ripple current is optional on
  every request; run `scripts/audit_material_core_database.py` for the
  current live count).
- **DCR / total wire length** (`winding.resistanceStatus`) and **DC
  copper loss** (`losses.copperLossStatus`) — `Evaluated` for most
  candidates via a real, geometry-derived mean-length-per-turn estimate,
  `not_evaluated` only for the subset of cores whose upstream geometry
  doesn't support it. DCR now includes lead/routing/connection resistance
  (`DesignRules` allowances), not just core-winding resistance.

**Recommendation tier** (`recommendation.tier`): `Phase1Recommended` |
`PreliminaryCandidate` | `Rejected`, replacing the old frontend-only
"Recommended" UI sugar. `Rejected` always mirrors `passed`. No real Phase 1
request reaches `Phase1Recommended` today, since `ThermalValidation`
always sets `isPreliminaryEstimate: true` — see FORMULAS.md section 12.

**Ranking:** passing candidates are ranked by tier first, then by real
known evaluated loss (copper + core, whichever are `Evaluated`,
`lossSummary.knownEvaluatedLossW`), predicted temperature rise,
manufacturability margin, saturation margin, current-density margin, area
product, and finally part number — see FORMULAS.md section 12 for the full
comparator. A missing number in any tiebreaker ranks as the worst case for
that dimension, never the best.
