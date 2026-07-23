# Worked Example: Mode 1 (Buck Converter Requirement Derivation)

Every number below came from an actual run of the tool against the real
`data/real_cores.csv` / `data/real_materials.csv` / `data/real_core_loss_coefficients.csv`
snapshot - nothing here is invented for illustration. Unlike
[TESTRESULTSMEAN.md](TESTRESULTSMEAN.md) (a historical Mode 2 example against
a since-deleted single-pick pipeline), this example starts from **Mode 1** -
a Buck converter's operating point, not a pre-known inductor requirement -
and is reproducible today against the live `/topology-design/buck` and
`/inductor-design` endpoints.

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

## 2. Stored Energy and Required Area Product

```
E  = 0.5 x L x Ipk^2 = 0.5 x 0.0000024 H x 44^2                = 2.323 mJ
Ap = (2 x E x 10^4) / (Ku x Bmax x J)
   = (2 x 2.323e-3 x 10^4) / (0.4 x 0.30 x 400)                = 0.968 cm^4
```

`Ku = 0.4`, `Bmax = 0.30 T` (Phase 1 default, only used where a material
has no measured value), `J = 400 A/cm^2` - all three read from
`activeRules` in the response, never hard-coded in the route layer.

This run returned **5 passing candidates, 7 rejected** (12 evaluated
total). As in the Mode 2 example, we follow the first candidate in each
list - the closeness of the two is exactly what makes it instructive.

---

## 3. Two Real Cores, One Turn Each, One Diverges on the Gap

Because the target inductance here is small (2.4 uH) against a database of
comparatively large, high-permeability cores, **both candidates seed to a
single turn** (`N = round(sqrt(L/AL_catalog))` rounds to 1 for each).
That's real, not simplified for the example - it's what happens when the
required inductance is low relative to the available cores' catalog AL.

| | First passing candidate | First rejected candidate |
|---|---|---|
| Core | `9498115002*` (material 98) | `B64290L0674X049` (N49) |
| Material source | Fair-Rite, Broadband family | TDK, N family |
| mu_r | 2258.05 | 1469.0 |
| Ae | 233.49 mm^2 | 97.50 mm^2 |
| Wa | 274.97 mm^2 | 408.28 mm^2 |
| Le | 97.35 mm | 92.68 mm |
| Catalog AL | 6805.54 nH/turn^2 | 1942.07 nH/turn^2 |
| Area product | 6.42 cm^4 (meets 0.968 cm^4) | 3.98 cm^4 (meets it) |

Both easily clear the 0.968 cm^4 requirement - area product alone doesn't
separate them.

**Turns** (identical seed):
```
Passing:  N = round(sqrt(2400 nH / 6805.54 nH/turn^2)) = round(sqrt(0.353)) = round(0.594) = 1
Rejected: N = round(sqrt(2400 nH / 1942.07 nH/turn^2)) = round(sqrt(1.236)) = round(1.112) = 1
```

**Gap - this is where they diverge:**
```
Passing candidate gap  = 0.4*pi x 1^2 x 2.3349cm^2 x 10/2400 - 9.735/2258.05
                        = 0.01221 - 0.00431              = 0.00795 cm -> rounds UP to 0.08 mm

Rejected candidate gap = 0.4*pi x 1^2 x 0.9750cm^2 x 10/2400 - 9.268/1469.0
                        = 0.00510 - 0.00631              = negative -> rounds DOWN to 0.00 mm (no gap)
```

The rejected candidate's required gap comes out **negative** - its
ungapped catalog AL is already lower than what 1 turn needs, so the
solver applies no gap at all and the core keeps its full catalog AL. The
passing candidate needs (and gets) a real, if small, 0.08 mm gap, which
lowers its effective AL below catalog.

**Resulting inductance** (`L = N^2 x AL_eff`, and `N = 1` here so this is
just `AL_eff` itself, in nH):
```
Passing:  AL_eff = 2383.26 nH/turn^2 -> L = 2.383262 uH -> error = -0.697%  (within +-10%)
Rejected: AL_eff = 1942.07 nH/turn^2 (= catalog, no gap) -> L = 1.942068 uH -> error = -19.081% (outside +-10%)
```

The rejected candidate fails **InductanceValidation** outright here - not
a close call on this check. But the more interesting failure is next.

---

## 4. Why the Rejected Candidate Also Saturates

```
Bpk = L(H) x Ipk(A) / (N x Ae(m^2))
```

| | First passing candidate | First rejected candidate |
|---|---|---|
| Calculated inductance | 2.383 uH (-0.70%) | 1.942 uH (-19.08%) |
| Peak flux density (Bpk) | 0.4491 T | 0.8764 T |
| Material Bmax (`BmaxT`, material-specific) | 0.501 T | 0.4914 T |
| PeakFluxValidation | PASS (under limit) | **FAIL - over the limit** |
| Saturation margin `(Bmax-Bpk)/Bmax` | 10.36% (>= 10% required) | **-78.35%** |
| SaturationValidation | PASS | **FAIL** |

Unlike the Mode 2 example (where the rejected candidate stayed just under
the hard limit and failed only on margin), this rejected candidate's flux
density **exceeds its own material's saturation limit outright** - not a
thin-margin case, a real overshoot. The root cause traces the same way it
did in the Mode 2 example: no gap was applied, so the core kept its full,
higher, ungapped AL for the same single turn, and that extra inductance
directly inflates `Bpk` in the formula above. This candidate is rejected
by three independent checks at once (`InductanceValidation`,
`PeakFluxValidation`, `SaturationValidation`) - a direct illustration of
the project's rule that **every failed check is reported, not just the
first one found**.

---

## 5. Winding and Losses (First Passing Candidate)

```
requiredAreaMm2 = Irms / J x 100 = 40.07 / 400 x 100 = 10.02 mm^2
```

A single AWG18 strand (0.823 mm^2) is far too small for 40 A - the
winding design falls back to **13 parallel strands of AWG18**
(`DesignRules.minimumSingleStrandAwg`), for a total conductor area of
13 x 0.823 = 10.70 mm^2:

```
Fill factor      = (turns x conductorArea x strands) / Wa
                 = (1 x 0.823 x 13) / 274.97                = 3.89%   (limit 60%) - PASS
Current density  = Irms / (conductorArea x strands)
                 = 40.07 / (0.823 x 13)                     = 3.745 A/mm^2  (limit 4.00) - PASS
```

DCR came from a real geometry-derived mean-length-per-turn (`mltMm =
63.1 mm` for this core, see [DATA_FILES.md](DATA_FILES.md)):

```
DCR = 0.0001017 ohms   (total wire length 0.820 m)
Pcu = Irms^2 x DCR = 40.07^2 x 0.0001017          = 0.163 W
```

**Core loss:** `not_evaluated` for this specific candidate - material 98
has coefficients in `data/real_core_loss_coefficients.csv` at other
frequencies, but not one covering 500 kHz, so `CoreLoss.cpp` correctly
reports it can't compute a real number rather than guessing. (The
rejected N49 candidate, for contrast, *does* have coefficients at this
frequency and returns a real - and very large, 36.3 W - core loss number,
which is itself further evidence of how poor a choice that candidate
would be, on top of already failing three validation checks.)

**Thermal:** `not_evaluated` - no thermal-resistance model or
surface-area data exists in either CSV yet (see [WORKFLOW.md](WORKFLOW.md)'s
Stage 4+ section).

---

## 6. The Two Outcomes, Side by Side

| | First passing candidate | First rejected candidate |
|---|---|---|
| Core | `9498115002*` | `B64290L0674X049` (N49) |
| Turns / Gap | 1 turn, 0.08 mm | 1 turn, 0.00 mm (no gap) |
| Calculated L (error) | 2.383 uH (-0.70%) | 1.942 uH (-19.08%) |
| Peak flux vs. limit | 0.449 T / 0.501 T - PASS | 0.876 T / 0.491 T - **FAIL** |
| Saturation margin | 10.36% - PASS | -78.35% - **FAIL** |
| Fill factor / current density | 3.89% / 3.745 A/mm^2 - both PASS | 2.62% / 3.745 A/mm^2 - both PASS |
| Copper loss | 0.163 W (real DCR) | 0.111 W (real DCR) |
| Core loss | not_evaluated (no coefficients at 500 kHz) | 36.27 W (real, and it's rejected anyway) |
| Result | **PASS** | **REJECTED** - 3 failed checks |

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
then **Generate Recommendation**.
