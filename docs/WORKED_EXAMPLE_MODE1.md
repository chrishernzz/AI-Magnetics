# Worked Example: Mode 1 (Buck Converter Requirement Derivation)

Every number below came from an actual run of the tool against the real
`data/real_cores.csv` / `data/real_materials.csv` / `data/real_core_loss_coefficients.csv`
/ `data/dc_bias_curves.csv` snapshot - nothing here is invented for
illustration. This example starts from **Mode 1** - a Buck converter's
operating point, not a pre-known inductor requirement - and is reproducible
today against the live `/topology-design/buck` and `/inductor-design`
endpoints.

The request below is the web UI's own default Buck-mode input, so opening
the tool, switching to "I know my Buck converter requirements," and
clicking through reproduces this exact walkthrough.

---

## 1. The Buck Converter Request

| Field | Value | Meaning |
|---|---|---|
| `vinMinV` | 36 | Minimum input voltage |
| `vinMaxV` | 60 | Maximum input voltage - sizing point (see below) |
| `voutV` | 12 | Regulated output voltage |
| `ioutA` | 40 | Output (load) current |
| `switchingFreqKHz` | 500 | Switching frequency |
| `rippleCurrentPercent` | 20 | Target inductor ripple, as % of `ioutA` |
| `ambientTemperatureC` | 25 | Passed through unchanged |
| `allowableTempRiseC` | 40 | Passed through unchanged |
| `inductanceTolerancePercent` | not supplied | defaults to 10% |

**POST `/topology-design/buck`** evaluates this at the worst-case point,
`Vin = vinMaxV` (see [WORKFLOW.md](WORKFLOW.md)'s Stage 0 section
for why the maximum, not minimum or a sweep, is correct here):

```
D (duty cycle)   = Vout / Vin_max        = 12 / 60          = 0.2
ripple_A         = Iout x ripple%        = 40 x 0.20        = 8 A
L                = (Vin - Vout) x D / (fsw x ripple_A)
                 = (60 - 12) x 0.2 / (500,000 x 8)           = 2.4 uH
Ipeak            = Iout + ripple_A / 2   = 40 + 4            = 44 A
```

**Response** (this is the exact JSON `/topology-design/buck` returns -
already shaped as an `InductorDesignRequest`, ready to feed into
`/inductor-design`):

```json
{
  "inductanceUH": 2.4,
  "peakCurrentA": 44.0,
  "switchingFreqKHz": 500.0,
  "ambientTemperatureC": 25.0,
  "allowableTempRiseC": 40.0,
  "inductanceTolerancePercent": null,
  "averageCurrentA": 40.0,
  "rippleCurrentPeakToPeakA": 8.0,
  "dutyCycle": 0.2
}
```

`rmsCurrentA` is deliberately left unset - it's derived one stage later,
identically to how Mode 2 derives it when a user supplies average current
and ripple instead of RMS directly:

```
Irms = sqrt(Iavg^2 + ripple^2/12) = sqrt(40^2 + 8^2/12) = sqrt(1605.33) = 40.07 A
```

From here on, this request is indistinguishable from one a Mode 2 user
typed in directly - every stage below is the same pipeline, same code,
same rules, regardless of which mode produced the input.

---

## 2. Material Candidates and Ranking Result

This run returned **159 passing candidates, 596 rejected** (755 evaluated
total - every core in the current Magnetics-only database is a
distributed-gap powder material, so every candidate takes the DC-bias
roll-off path in Section 3 below). We follow the top-ranked passing
candidate and one representative rejected candidate.

---

## 3. Top-Ranked Candidate: Turns Fixed at Zero-Bias, DC-Bias Roll-Off Applied

| | Top-ranked candidate |
|---|---|
| Core | `00G8044E026` |
| Material | `Edge 26` (powder E-core, mu_r = 26) |
| Ae | 389.0 mm^2 |
| Wa | 1362.0 mm^2 |
| Le | 208.0 mm |
| Catalog AL | 91.0 nH/turn^2 |
| Area product | 52.98 cm^4 |

Because every core in the current database is a distributed-gap (powder)
material, `TurnsAndGapDesign.cpp` takes `solveDistributedGapCore()`, not
the machined-gap series-reluctance path: turns are solved **once** from
the zero-bias catalog AL and then held fixed - there is no gap to cut for
these materials, so `gapMm` is always `0.0` and `gapMethod` reports
`Distributed`.

```
N = round(sqrt(L_target_nH / AL0)) = round(sqrt(2400 / 91.0)) = round(5.13) = 5
calculatedInductanceUH (zero-bias) = N^2 x AL0 x 1e-3 = 25 x 91.0 x 1e-3 = 2.275 uH
inductanceErrorPercent = (2.275 - 2.4) / 2.4 x 100 = -5.21%   (within +-10% tolerance -> PASS)
```

**DC-bias roll-off**, applied against those same fixed 5 turns at the real
44 A peak current:

```
H (Oe)  = 0.4*pi * N * Ipk / Le_cm = 0.4*pi * 5 * 44 / 20.8 = 13.29 Oe
```

`Edge 26` has a published roll-off curve in `data/dc_bias_curves.csv`
(`usesDCBiasRolloffCurve: true`). At this H, the real percent-of-catalog
permeability retained comes back at **99.9986%** - negligible roll-off at
this low field strength - so `loadedInductanceUH` (the real inductance
this winding delivers at 44 A) is essentially unchanged from the zero-bias
value: **2.2750 uH**, vs. 2.275 uH at 0 A.

---

## 4. Validation Results (Top-Ranked Candidate)

| Check | Status | Value | Limit | Result |
|---|---|---|---|---|
| CurrentConsistencyValidation | Evaluated | 36.0 | - | PASS (diagnostic only) |
| InductanceValidation | Evaluated | 5.21% error | 10% | **PASS** |
| PeakFluxValidation | Evaluated | 0.0515 T | 1.5 T (material `BmaxT`) | **PASS** |
| SaturationValidation | Evaluated | 96.57% margin | 10% required | **PASS** |
| WindingFitValidation | Evaluated | 3.88% physical fill | 60% max | **PASS** |
| CurrentDensityValidation | Evaluated | 6.95 A/mm^2 | 9.87 A/mm^2 | **PASS** |
| BundleFitValidation | Evaluated | 7.52 mm bundle | 34.37 mm opening | **PASS** |
| ThermalValidation | Evaluated | 21.32 C rise | 40 C allowed | **PASS** (preliminary) |

`PeakFluxValidation`/`SaturationValidation` are computed from
`loadedInductanceUH` (the real, DC-bias-corrected inductance at 44 A), not
`calculatedInductanceUH` (the zero-bias design value) - saturation risk
has to be judged against what the core actually does at real current.

**Winding:** 7x AWG18 parallel strands, single-build magnet wire. Cold DCR
= 0.001895 ohm; copper loss = 3.357 W (`Evaluated`, real geometry-derived
MLT). Core loss = 2.627 W (`Evaluated` - `Edge 26` has real Steinmetz
coefficients covering 500 kHz).

**Thermal:** converges to a 21.32 C predicted rise, `PreliminaryThermalEstimate`,
`thermalResistanceUsesRealSurfaceArea: false` - this specific part's
wound-coil surface area hasn't been transcribed yet, so the estimate used
the Ae x Le shape-factor fallback (Tier 2), not a real datasheet number
(Tier 1). See [FORMULAS.md](FORMULAS.md) section 10.

**Recommendation tier:** `ConditionalPass` - every mandatory check passed,
but `ThermalValidation` always carries `isPreliminaryEstimate: true` (no
fully-measured thermal model exists), which caps every real candidate at
`ConditionalPass`, never `Pass`.

---

## 5. A Representative Rejected Candidate: Two Real Failures

| | Rejected candidate |
|---|---|
| Core | `0055138AY` |
| Material | `MPP 160` (powder toroid, mu_r = 160) |
| Ae | 1.3 mm^2 |
| Wa | 1.63 mm^2 |
| Le | 8.06 mm |
| Catalog AL | very high relative to size (small toroid, high permeability) |

```
N = round(sqrt(2400 / AL0)) = 9 turns
calculatedInductanceUH (zero-bias) = 2.673 uH
inductanceErrorPercent = +11.375%   (outside +-10% tolerance -> FAIL)
```

This core's window is tiny (`Wa` = 1.63 mm^2) relative to the copper this
design needs for 40 A - `WindingFitValidation` reports a **physical fill
of 4963%** against a 60% limit, a real, honest, physically-impossible-to-build
number, not a formula error: this specific core is simply far too small
for this current, and the tool says so plainly rather than clamping the
number to something that looks more reasonable.

At H = 13.29 Oe (same formula as Section 3, different core/turns), MPP
160's real DC-bias curve reports only **0.128%** permeability retained -
severe roll-off - so `loadedInductanceUH` collapses to 0.0034 uH, a
fraction of the 2.4 uH target. `BundleFitValidation`/`ThermalValidation`
never even run (`NotEvaluated`) since the winding design itself already
failed.

```
rejected: InductanceValidation - calculated inductance 2.673 uH vs target
  2.4 uH (error 11.375%), tolerance 10.000%
rejected: WindingFitValidation - physical window fill 4963.47% vs
  maximum 60.00% (raw copper-only fill was 3180.92%)
```

Two independent, real failures on the same candidate - exactly the
project's rule that **every failed check is reported, not just the
first one found**.

---

## 6. Why the Top-Ranked Candidate Beat 158 Other Passing Candidates

Ranking (`candidateRanksAhead()`, see [FORMULAS.md](FORMULAS.md) section 12)
sorts tier first, then DC-bias permeability retention (higher is better),
then known evaluated loss, predicted temperature rise, manufacturability
margin, saturation margin, current-density margin, area product, and part
number as a final tiebreak. `00G8044E026` isn't the only candidate near
100% retention at this operating point - low H at 44 A across a
comparatively large core keeps roll-off negligible for many candidates -
so the real differentiator among the near-100%-retention group is known
loss and thermal rise, both of which this candidate wins on.

## How to Reproduce This

```bash
curl -X POST http://localhost:8000/topology-design/buck -H "Content-Type: application/json" -d '{
  "vinMinV": 36, "vinMaxV": 60, "voutV": 12, "ioutA": 40,
  "switchingFreqKHz": 500, "rippleCurrentPercent": 20,
  "ambientTemperatureC": 25, "allowableTempRiseC": 40
}'
```

Take that response's fields directly as the body of `POST /inductor-design`
(they're already the right shape - see [API_REFERENCE.md](API_REFERENCE.md)),
or just use the web UI: switch to "I know my Buck converter requirements,"
leave the pre-filled defaults, click **Calculate Magnetic Requirements**,
then **Generate Recommendation**. Check the DC-Bias Physics tab on the
top-ranked candidate to see the roll-off curve/retention numbers rendered
the same way as Sections 3-4 above.
