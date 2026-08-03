# Formula Reference

This document exists for one reason: so that someone who opens this codebase
without having written it can understand **every equation the engine uses,
why that equation and not a different one, and what every parameter in it
means and does.** If you're new to magnetics design, or you're reviewing
this project and need to explain "why does the code do this math," start
here.

Each section below covers one formula (or tightly related group of
formulas): what it computes, why it's the right tool for the job, the
formula itself, every parameter it uses, and how the result feeds the next
stage of the pipeline. See [WORKFLOW.md](WORKFLOW.md) for the stage-by-stage
pipeline narrative and [ARCHITECTURE.md](ARCHITECTURE.md) for which file
implements what — this document is specifically about the *math*.

---

## 0. The Big Picture: Why So Many Formulas?

Sizing a power inductor is not one calculation — it's a chain of decisions
that each depend on the one before it:

```
How much energy must it store?
  -> How physically big does the core need to be?
    -> Which real core, of the ones available, is big enough?
      -> How many turns of wire, and how much air gap, actually produce
         the target inductance on that specific core?
        -> Does that design saturate the core, overheat the wire, or
           overflow the winding window?
          -> How much power does it waste as heat?
```

Every formula in this document answers one link in that chain. Skipping a
link (for example, picking a core without checking that it actually
produces the right inductance once wound) is exactly how the *old* version
of this tool produced designs that looked plausible but weren't verified.
The point of writing every formula down here is so nobody has to trust
"the code does something reasonable" — you can check.

---

## 1. RMS Current from Ripple

**File:** `src/backend/services/RequirementDerivationService.cpp`

### What it's for

The engine needs RMS current for anything related to heating (wire sizing,
copper loss) — but a user (or an upstream converter-design step) often only
knows the *average* current and how much the current ripples up and down,
not the RMS value directly. This formula fills that gap **only when RMS
current isn't supplied directly.**

### Why this formula

For a current waveform that rises and falls linearly (a triangle wave
riding on a DC average — the standard shape of inductor current in a
switching converter operating in continuous conduction), the RMS value has
a closed-form solution. This is a standard result from AC circuit theory,
not something invented for this project.

```
Irms = sqrt( Iavg^2 + ripple^2 / 12 )
```

### Parameters

| Symbol | Meaning | Unit | Where it comes from |
|---|---|---|---|
| `Irms` | Root-mean-square current — the "heating-equivalent" current value | A | **Output** — used everywhere current-dependent heating matters |
| `Iavg` | Average (DC) current through the inductor | A | User input: `averageCurrentA` |
| `ripple` | Peak-to-peak ripple current (the full swing from the current's low point to its high point) | A | User input: `rippleCurrentPeakToPeakA` |

### Why it matters if this is wrong

If the real current waveform isn't triangular (for example, discontinuous
conduction, or a converter with a very different duty cycle behavior), this
formula gives the wrong RMS value — which then produces a wire gauge and
copper-loss estimate that's wrong too. That's why the engine **only** uses
this derivation when RMS current isn't supplied directly, and every result
that came from it says so explicitly in the response (`rmsCurrentDerived:
true` plus a plain-language assumption string). If you know the real
waveform isn't triangular, supply `rmsCurrentA` directly and this formula
is never used.

### Feeds into

Winding design (Section 6) and DC copper loss (Section 8).

---

## 2. Stored Energy and Required Area Product (Ap)

**File:** `src/core/sizing/AreaProduct.cpp` — this is the **McLyman area-product
method**, from Colonel William McLyman's *Transformer and Inductor Design
Handbook*, the standard reference text for this kind of sizing.

### What it's for

Before you can pick a core, you need to know roughly how physically big it
has to be. The area-product method answers that with a single number (Ap,
in cm⁴) that combines two things every core has: how much magnetic flux it
can carry (cross-sectional area) and how much copper fits around it
(window area). A bigger Ap means a bigger, more capable core.

### Why this formula

This is the industry-standard way to size an inductor core from first
principles (stored energy) before touching a parts catalog, because it
doesn't require guessing a core first — it derives the *requirement*, then
the catalog search (Section 3) becomes a simple filter instead of trial and
error.

```
E = 0.5 * L * Ipk^2

Ap = (2 * E * 10^4) / (Ku * Bmax * J)
```

### Parameters

| Symbol | Meaning | Unit | Where it comes from |
|---|---|---|---|
| `E` | Peak stored energy in the magnetic field | J (joules) | **Output** of the first line, input to the second |
| `L` | Target inductance | H | User input: `inductanceUH` (converted to henries) |
| `Ipk` | Peak current — the highest instantaneous current the inductor sees | A | User input: `peakCurrentA`. **Never** the same as RMS current — see Section 1 |
| `Ap` | Required area product | cm⁴ | **Output** — the number every candidate core is checked against (Section 3) |
| `Ku` | Window utilization factor — the fraction of the core's window area realistically usable for copper once insulation, bobbin walls, and winding margin are accounted for | dimensionless (0-1) | `DesignRules.windowUtilization`, default 0.4 |
| `Bmax` | Maximum allowed peak flux density | T (tesla) | `DesignRules.defaultFluxDensityLimitT`, default 0.30 T — a conservative default, not a material-specific saturation point (see Section 7) |
| `J` | Allowable current density — how much current per unit of copper cross-section is considered safe before overheating | A/cm² | `DesignRules.allowableCurrentDensityAperCm2`, default 400 |

### Why it matters if this is wrong

Every one of Ku, Bmax, and J is a policy choice, not a law of physics —
that's exactly why they live in one named, documented place
(`DesignRules::phase1Default()`) instead of being buried as magic numbers
in a route handler. A too-aggressive `J` undersizes the core and the real
part overheats; a too-conservative one oversizes it and wastes cost and
board space. Because these three numbers are returned in every API
response as `activeRules`, nobody has to guess what assumption produced a
given Ap value.

### Feeds into

Core candidate evaluation (Section 3).

---

## 3. Core-Side Area Product (Does This Core Meet the Requirement?)

**File:** `src/core/sizing/CoreEvaluation.cpp`

### What it's for

Section 2 tells you how big a core needs to be. This formula computes how
big a *specific, real* core actually is, so the two numbers can be
compared.

### Why this formula

Ap is defined the same way on both sides of the comparison (cross-section
area times window area) specifically so they're an apples-to-apples check.

```
Ap_core = (Ae * Wa) * 1e-4
meetsAreaProduct = Ap_core >= Ap_required * 0.95
```

### Parameters

| Symbol | Meaning | Unit | Where it comes from |
|---|---|---|---|
| `Ap_core` | This specific core's actual area product | cm⁴ | **Output** |
| `Ae` | Core effective cross-sectional area — the area flux actually flows through | mm² | Core database (`data/real_cores.csv`) |
| `Wa` | Core window area — the open space available for winding | mm² | Core database |
| `1e-4` | Unit conversion (mm² x mm² = mm⁴, and 1 cm⁴ = 1e4 mm⁴) | — | fixed conversion constant, not a design choice |
| `0.95` | 5% safety margin | — | A deliberate small buffer so a core that's *technically* one rounding error short isn't rejected outright |

### Why it matters if this is wrong

This is the check that replaced the old behavior of silently handing back
the largest available core when nothing actually fit. Every core — passing
or not — is kept and labeled with its own `meetsAreaProduct` flag, so if
*nothing* passes, the final response can report exactly how big a core
would have been needed versus the biggest one actually available
(`requiredAreaProductCm4` / `largestAvailableAreaProductCm4`), instead of
quietly recommending something undersized.

### Feeds into

Turns and gap design (Section 4) — but only for cores that pass this check.

---

## 4. Turns and Air Gap (Gapped-Core Magnetic Circuit)

**Files:** `src/core/magnetics/GapDesign.cpp` (the physics), `src/core/
TurnsAndGapDesign.cpp` (the process that uses it)

### What it's for

A catalog core has a fixed, ungapped inductance-per-turn value (AL). Most
practical power inductors need an intentional air gap cut into the core to
store energy without saturating — inserting a gap trades some
inductance-per-turn for a lot more energy-storage headroom. This section
answers two connected questions: *how much gap*, and *how many turns*, to
hit a specific target inductance on a specific core.

### Why this formula

This is the **series-reluctance model** of a gapped magnetic core: a
standard magnetic-circuit method that treats the core material and the air
gap as two resistances-to-flux ("reluctances") in series, the same way
resistors in series add in an electrical circuit. It's the same method
McLyman's handbook uses, and it's the correct model whenever a gap is small
relative to the core's overall path length (true for essentially every
practical power-inductor gap).

```
AL0 (ungapped) = 0.4*pi * muR * (Ae_cm2 / Le_cm) * 10

gap_cm = 0.4*pi * N^2 * Ae_cm2 * 10 / L_target_nH  -  Le_cm / muR

AL_eff (gapped) = 0.4*pi * Ae_cm2 * 10 / (Le_cm/muR + gap_cm)
```

### Parameters

| Symbol | Meaning | Unit | Where it comes from |
|---|---|---|---|
| `AL0` | Ungapped inductance factor — how much inductance one turn produces with no gap | nH per turn² | Core database, or computed from `muR`/`Ae`/`Le` |
| `AL_eff` | Effective (gapped) inductance factor | nH per turn² | **Output**, recomputed every iteration |
| `muR` | Core material's relative permeability — how much better the core conducts magnetic flux than air | dimensionless | Core database |
| `Ae_cm2` | Core effective area, converted to cm² | cm² | Core database (`Ae` in mm² / 100) |
| `Le_cm` | Core magnetic path length, converted to cm | cm | Core database (`Le` in mm / 10) |
| `N` | Number of turns | integer, turns | Estimated, then refined — see the convergence process below |
| `gap_cm` | Required air gap length | cm | **Output** — this is what a technician would actually cut/insert |
| `L_target_nH` | Target inductance, in nanohenries | nH | User input: `inductanceUH` x 1000 |
| `10`, `0.4*pi` | Unit-system constants from the CGS magnetic-circuit formula (`0.4*pi` is `mu0` in CGS units) | — | Fixed physics constants, not design choices |

**A note on accuracy:** this model does not include fringing-flux
correction (a secondary effect where some flux bulges out around the gap
edges instead of staying inside the core). No data loaded into this
project currently includes what a fringing correction needs (winding/
bobbin geometry), so it's a documented simplification, not a silent
omission — it was **verified numerically against a real manufacturer's own
catalog AL value to within 0.03%** (see `tests/cpp/EngineTests.cpp`), which
is well within the precision this design phase needs.

### Why turns and gap can't be solved independently

Look at the gap formula: it needs `N` (turns) to solve for the gap. But the
gapped AL formula needs the gap to tell you how many turns you actually
need. Each one depends on the other's answer. That's why this isn't a
single formula — it's a loop:

```
1. Estimate a starting turns count from the core's ungapped AL
   (N = round(sqrt(L_target_nH / AL0)) — reusing the original
   TurnsCalculation.cpp formula as the seed).
2. Solve the gap needed for that turns count.
3. Recompute the gapped, effective AL with that gap.
4. Recompute how many turns are needed at that new AL.
5. If the turns count from step 4 matches step 1 (or the previous
   iteration), it's converged. If not, go back to step 2 with the new
   turns count.
```

This typically settles in 2-4 iterations. A design is **rejected** — not
silently forced — if the required gap would be impractically large (more
than `DesignRules.maxGapFraction`, default 40%, of the core's own magnetic
path length) or if it doesn't converge within 15 iterations. An exhaustive
2,000,000-trial simulation of this exact iteration across wide parameter
ranges found zero cases where it fails to converge without also hitting
that excessive-gap rejection first — the turns/gap feedback is structurally
self-correcting (see `tests/cpp/GapToleranceTests.cpp`).

### Gap-tolerance sweep and small-gap warning

A gap that lands on target as-designed can still fail once realistic
mechanical tolerance is accounted for. After the loop above converges, the
gap is swept ±`DesignRules.gapTolerancePercent` (default 10%) and the
inductance is recomputed at both extremes (turns held fixed):

```
gapMin = gap * (1 - gapTolerancePercent/100)
gapMax = gap * (1 + gapTolerancePercent/100)
```

If either extreme falls outside the requested inductance tolerance, the
design is rejected with a reason naming which extreme(s) failed
(`TurnsAndGapResult.inductanceWithinToleranceAcrossGapRange`) — a nominal
pass alone is not sufficient. Separately, a calculated gap below
`DesignRules.minManufacturableGapMm` (default 0.05mm) sets
`smallGapWarning: true` with a plain-language reason — a caveat, not a
rejection, since the design is still physically valid, just harder to
reliably machine or lap by hand.

Only `GapMethod.MachinedCenterLeg` has a validated formula in Phase 1 —
`DesignRules.gapMethod` set to `Spacer`, `Distributed`, or
`ManufacturerGapped` is rejected outright rather than having this one
validated formula silently applied to a technique it was never checked
against.

### Why it matters if this is wrong

This is the single most important check the old version of the tool
skipped entirely: it recommended a core and a material but never verified
that any achievable number of turns and gap actually produced the
requested inductance. Getting turns/gap wrong means the real, physical part
doesn't have the inductance it was designed for.

### Feeds into

Magnetic validation (Section 5) and winding design (Section 6), both of
which need the final turns count.

---

## 5. Magnetic Validation

**File:** `src/validation/DesignValidation.cpp`

This section covers three of the six named validation checks that involve
their own formulas (the other three — WindingFitValidation,
CurrentDensityValidation, ThermalValidation — are covered in Sections 6, 6,
and 9 respectively, since their formulas belong to those other stages).

### 5.1 Peak Flux Density

**What it's for:** confirms the design doesn't push the core past a safe
flux density, which is what causes magnetic saturation (the point where a
core stops being able to store more energy and inductance collapses).

**Why this formula:** flux linkage (`L * Ipk`) must equal the flux through
the core times the number of turns times the core area — rearranging that
relationship for flux density is the standard way to compute peak flux
from the inductance, current, and geometry already known at this point in
the pipeline.

```
Bpk = L(H) * Ipk(A) / (N * Ae(m^2))
```

| Symbol | Meaning | Unit | Source |
|---|---|---|---|
| `Bpk` | Calculated peak flux density | T | **Output** |
| `L` | The *calculated* inductance from Section 4 (not just the target) | H | `TurnsAndGapResult.calculatedInductanceUH` |
| `Ipk` | Peak current | A | User input |
| `N` | Turns count from Section 4 | turns | `TurnsAndGapResult.turns` |
| `Ae` | Core effective area, converted to m² | m² | Core database (`Ae` in mm² x 1e-6) |

### 5.2 Saturation Margin

**What it's for:** it's not enough for `Bpk` to be merely under the limit
— real-world variation (temperature, manufacturing tolerance, transient
overcurrent) needs headroom. This check enforces a minimum buffer.

```
margin% = 100 * (Blimit - Bpk) / Blimit
```

| Symbol | Meaning | Unit | Source |
|---|---|---|---|
| `margin%` | How much headroom exists below the flux limit | % | **Output** — must be >= `DesignRules.minimumSaturationMarginPercent` (default 10%) |
| `Blimit` | The applicable flux density limit | T | Material-specific `BmaxT` if the material has one (real data for all 165 materials as of the current snapshot), otherwise `DesignRules.defaultFluxDensityLimitT` |

**Important:** whenever `Blimit` falls back to the Phase 1 default instead
of a real material-specific number, the result explicitly flags
`usesDefaultAssumption: true` — so a generic assumption is never presented as a
measured fact about a specific material.

### 5.3 Flux-Limit Tiers (Informational)

`calculateFluxLimitTiers()` (`src/validation/DesignValidation.cpp`) breaks
the single limit above into four named tiers, surfaced per candidate as
`fluxLimits` — purely informational, it does not change what 5.1/5.2 gate
on. Only two of the four are real:

```
absoluteSaturationT      = Blimit (same value as 5.2)
recommendedOperatingT    = absoluteSaturationT * DesignRules.recommendedFluxDerateFactor  (default 0.85)
temperatureAdjustedStatus  = always NotEvaluated (no temp-coefficient-vs-saturation data exists)
coreLossLimitedStatus      = always NotEvaluated (no core-loss-vs-flux-density sweep data exists)
```

### Feeds into

The candidate's overall pass/fail decision. A failed check here blocks the
candidate and its explanation appears in `rejectionReasons`.

---

## 6. Winding Design

**File:** `src/core/winding/WindingDesign.cpp`, using the standard AWG (American
Wire Gauge) reference table in `src/data/AwgTable.h`

### What it's for

Turns the turns count (Section 4) and RMS current (Section 1) into an
actual, buildable wire specification: gauge, how many parallel strands (if
any), and whether it physically fits in the core's window.

### Why these formulas

`Wa` (window area) is a fixed, known geometric fact about the core, and the
required copper cross-section is a direct consequence of the allowable
current density rule — no iteration needed here, unlike Section 4.

```
requiredAreaMm2 = Irms / J_Acm2 * 100

fillFactor = (turns * conductorAreaMm2 * strands) / Wa_mm2

currentDensity = Irms / (conductorAreaMm2 * strands)
```

### Parameters

| Symbol | Meaning | Unit | Source |
|---|---|---|---|
| `requiredAreaMm2` | Minimum copper cross-section needed | mm² | **Output** — used to pick an AWG gauge from the reference table |
| `Irms` | RMS current (Section 1) | A | `OperatingPoint.rmsCurrentA` |
| `J_Acm2` | Allowable current density | A/cm² | `DesignRules.allowableCurrentDensityAperCm2` |
| `100` | Unit conversion, A/cm² -> A/mm² | — | fixed conversion |
| `fillFactor` | Fraction of the window actually filled with copper | dimensionless (0-1) | **Output** — must stay under `DesignRules.maximumFillFactor` (default 0.6) |
| `conductorAreaMm2` | Cross-section area of *one* strand of the chosen AWG gauge | mm² | `src/data/AwgTable.h` (public NEMA MW1000 reference geometry) |
| `strands` | How many parallel strands of that gauge are used | integer | 1, unless a single strand would need to be thicker than `DesignRules.minimumSingleStrandAwg` (default AWG 18) allows — then the engine switches to multiple thinner parallel strands instead of one impractically thick wire |
| `currentDensity` | The *actual* current density once a real, discrete wire gauge has been chosen (may differ slightly from `J` above because gauges come in fixed steps) | A/mm² | **Output** — checked by CurrentDensityValidation |

### Why it matters if this is wrong

An undersized wire (fill factor or current density too high) overheats in
the real part; an oversized one doesn't fit the window at all. The AWG
table itself is public, standardized reference geometry — not a
project-specific assumption — so the only real design decision here is
`minimumSingleStrandAwg`, which is explicitly documented as a
manufacturability heuristic (how thick a single solid wire is practical to
hand-wind), separate from the wire geometry table it's compared against.

### Physical window fill (realistic, not raw copper)

`fillFactor` above is copper-only — it undercounts real winding space.
`physicalWindowFillFactor` is the figure `WindingFitValidation` actually
gates on:

```
insulatedDiameterMm  = bareStrandDiameterMm + DesignRules.singleBuildInsulationBuildUpMm
insulatedAreaMm2     = pi/4 * insulatedDiameterMm^2

physicalWindowAreaMm2 = Wa * bobbinWindowDerateFactor * (1 - marginAllowanceAreaFraction - leadExitAllowanceAreaFraction)
physicalCopperAreaMm2 = (turns * parallelStrands * insulatedAreaMm2) / packingFactor

physicalWindowFillFactor = physicalCopperAreaMm2 / physicalWindowAreaMm2
```

All four derate factors (`singleBuildInsulationBuildUpMm`,
`bobbinWindowDerateFactor`, `marginAllowanceAreaFraction`,
`leadExitAllowanceAreaFraction`, `packingFactor`) are Phase 1 generic
estimates in `DesignRules`, not per-core measured data — margin and
lead-exit are area *fractions*, not literal mm, because core data has no
width/height split, only a raw window area. `WindingFitValidation` gates
on `physicalWindowFillFactor`/`fitsPhysicalWindow`; the raw copper-only
`fillFactor`/`fitsWindow` remain in the response as an informational
figure. **This is an intentional behavior change**: a candidate that
passed on raw copper fill alone can now fail on the physical model.

Parallel-strand bundle-vs-narrowest-opening fit is permanently
`not_evaluated` (`bundleFitStatus`) for the same reason — no width/height
split exists to check a bundle against.

### Total wire length and DCR (lead/routing/connection included)

```
coreWindingLengthM = turns * mltMm / 1000          (single strand, around the core only)
leadLengthM        = DesignRules.totalLeadLengthAllowanceMm / 1000
routingLengthM     = DesignRules.routingLengthAllowanceMm / 1000
totalLengthM       = coreWindingLengthM + leadLengthM + routingLengthM

coldDcrOhmsAt20C = resistivity_ohm_m * totalLengthM / conductorArea_m2 / parallelStrands
                   + DesignRules.connectionResistanceMilliOhm / 1000
```

Computing these needs the core's mean-length-per-turn (MLT) - `data/real_cores.csv`'s
`Mlt` column, a real-geometry estimate from each core's central-column
cross-section (`scripts/export_real_data.py`; not accounting for bobbin
wall thickness or winding buildup - a documented simplification, same
policy as the gap formula's fringing-flux omission). `resistivity_ohm_m`
is annealed copper at 20°C (1.724e-8 Ω·m). Lead length, routing length,
and connection resistance are Phase 1 generic `DesignRules` estimates, not
per-part measured data — added because a real winding's DCR is more than
just the wire wound around the core. Fill factor and current density never
needed MLT (they only need turns and window area), so they're computed
either way; DCR is reported `not_evaluated` with an explicit explanation
for the subset of cores whose upstream geometry doesn't support an MLT
estimate.

`estimatedHotDcrOhms` starts as a conservative sanity-check estimate
(`coldDcrOhmsAt20C` corrected to `DesignRules.assumedWindingTempCWhenThermalNotEvaluated`,
default 90°C) and is overwritten with the real converged value from the
thermal loop (Section 10) whenever that loop converges.

### Feeds into

DC copper loss (Section 8) — only when DCR was actually computed. Thermal
evaluation (Section 10) — the cold DCR seeds the iterative loop.

---

## 7. Why Bmax Defaults to 0.30 T

This isn't a separate formula, but it's a parameter that appears in
Sections 2 and 5, so it's worth explaining on its own. `DesignRules.
defaultFluxDensityLimitT = 0.30 T` is a **conservative, generic ferrite
saturation guideline** — real ferrite materials often saturate well above
this, but 0.30 T is a commonly used safe design point when a material's
actual measured saturation curve isn't available. As of this data
snapshot, `data/real_materials.csv`'s `BmaxT` is real, material-specific
saturation flux density for all 34 materials (transcribed by hand from
Magnetics Inc.'s own catalog), so this default is now the exception
rather than the rule - it's only
used as a fallback for a material with no measured value. The engine never
presents it as if it were a measured fact about a specific material —
every check that uses it sets `usesDefaultAssumption: true` in its result, and
`usesDefaultAssumption: false` whenever a real material-specific `BmaxT` was
used instead.

---

## 8. DC Copper Loss

**File:** `src/core/losses/CopperLoss.cpp`, orchestrated by `src/core/
LossEvaluation.cpp`

### What it's for

Estimates how much power the winding wastes as heat from simple resistive
(I²R) heating.

### Why this formula

This is Ohm's-law power dissipation, applied with the correct current
value — **RMS current, not peak current** — because heating is a function
of the current's average heating effect over time, not its instantaneous
maximum.

```
Pcu = Irms^2 * DCR
```

| Symbol | Meaning | Unit | Source |
|---|---|---|---|
| `Pcu` | DC copper (resistive) power loss | W | **Output** |
| `Irms` | RMS current | A | `OperatingPoint.rmsCurrentA` |
| `DCR` | Winding DC resistance | ohms | From Section 6 — available when the core has a real MLT estimate |

### Why it matters if this is wrong

Using peak current here instead of RMS would overstate the loss
significantly for any real ripple waveform — this is exactly the kind of
current-field mixup the project's own rules explicitly forbid (see Section
1: peak and RMS current are never interchangeable).

### Current status

Real for cores with an MLT estimate (Section 6) — `copperLossStatus:
Evaluated` with a genuine watt value. Still `not_evaluated` for the subset
of cores whose upstream geometry doesn't support an MLT estimate, rather
than presenting `0 W` as if it were a real, computed answer.

---

## 9. Core Loss (Real Steinmetz Formula, Ripple-Gated)

**File:** `src/core/losses/CoreLoss.cpp`, orchestrated from `src/core/losses/LossEvaluation.cpp`

### What it's for

Estimates power lost to hysteresis and eddy currents inside the core
material itself (as opposed to the winding).

### The formula

Real core-loss modeling uses the Steinmetz equation, with `k`, `alpha`,
`beta` fitted per material from real measurement curves:

```
Pv (W/m³) = k * f^alpha * B^beta
```

| Symbol | Meaning | Unit | Source |
|---|---|---|---|
| `Pv` | Core loss density | **W/m³** (not W/cm³ — see caveat below) | **Output** |
| `k`, `alpha`, `beta` | Steinmetz coefficients | — | `data/real_core_loss_coefficients.csv`, looked up per material + frequency via `findCoreLossCoefficients()` |
| `f` | Switching frequency | Hz | User input |
| `B` (`fluxDensitySwingT`) | Flux density swing | T | Computed from ripple current — see below |

Total core loss in watts is `Pv * Ve`, where `Ve` is the core's effective
volume (`Ae_mm2 * Le_mm`, converted to m³).

**Current data state:** `data/real_core_loss_coefficients.csv` is empty as
of this snapshot — Magnetics does not publish Steinmetz (`k`/`alpha`/`beta`)
coefficients for MPP/Kool Mu-family materials the way ferrite vendors did
for the materials this project used to carry, so `findCoreLossCoefficients()`
never finds a match and core loss reports `NotEvaluated` for every
candidate today, not a fabricated zero.

**Units caveat, stated explicitly because it silently produced a
600,000 W result during development (historical - the coefficients that
triggered this have since been removed along with the rest of ferrite
scope, but the unit convention below still applies to any coefficients
added in the future):** these coefficients, when populated, follow SI
convention, so `Pv` is in **W/m³, not W/cm³**. The retired placeholder
formula this replaced used W/cm³; carrying that assumption over to the
real Steinmetz formula produced core-loss numbers six orders of magnitude
too large. Caught by sanity-checking the output against a physically
plausible loss density for a small ripple swing — not something a unit
test alone would have caught, since the formula itself was correct, only
the assumed output unit was wrong.

### Where the flux-density swing comes from (Option 1, chosen)

The formula needs the *AC* flux swing, not the DC-biased peak flux
already computed for saturation checks. Two options were on the table:
compute it only when the request supplies real ripple-current data, or
approximate it from the peak flux density (covers every request, but
would misrepresent a DC-biased inductor as swinging symmetrically around
zero). **Option 1 was chosen** — core loss is computed only when
`rippleCurrentPeakToPeakA` is supplied:

```
Bswing (T) = calculatedInductanceH * ripplePeakToPeakA / (turns * Ae_m²)
```

Same peak-flux relationship `PeakFluxValidation` uses (Section 5), driven
by ripple current instead of peak current. No ripple current supplied →
`coreLossStatus: not_evaluated`, same as always — never approximated.

### Current data coverage

Real Steinmetz coefficients exist for 29 of the 165 materials in use — the
ferrite families (3C9x, 78/79/80/95/98, N-series). The powder/Kool
Mµ/XFlux materials aren't characterized as Steinmetz upstream at all (a
real absence, not an export bug) and correctly stay `not_evaluated`
regardless of ripple current — `MaterialCandidate::hasCoreLossData`
reflects this per material.

### Saturation gate (found via real UI use, not caught by prior tests)

`Pv = k * f^alpha * B^beta` is only a valid small-signal fit *below* the
material's own saturation flux density — past that point the real B-H
curve goes nonlinear and the power-law formula has no physical meaning.
The intended guard for this is `fluxSwingWithinValidatedRange()`, which
checks a coefficient row's own `minFluxSwingT`/`maxFluxSwingT` — but
those columns don't exist in `data/real_core_loss_coefficients.csv` for
any material yet (see [DATA_FILES.md](DATA_FILES.md)), so that guard is
currently always a no-op. Without a second check, a candidate whose
flux swing genuinely exceeded its material's Bsat (few turns + a large
`rippleCurrentPeakToPeakA`, the same real mechanism `PeakFluxValidation`
watches for) would still get a Steinmetz value computed and reported —
confirmed live, producing core-loss figures in the tens of thousands of
watts for a small inductor, since the power law explodes for a B input
several times past where it was ever fit.

Fixed by gating on the material's real, already-loaded `bmaxT` (the same
value `SaturationValidation`/`PeakFluxValidation` use) in
`LossEvaluation.cpp`: if `fluxDensitySwingT > material.bmaxT`,
`coreLossStatus` stays `not_evaluated` with an explanation naming the
exact swing and the material's real Bsat, rather than reporting an
extrapolated number. This is a real, sourced bound — not a new
fabricated threshold — and it does not change any candidate's
pass/fail; `SaturationValidation`/`PeakFluxValidation` already reject
these designs independently.

**Known remaining gap, not fully closed by this fix:** BmaxT is the
material's absolute saturation point, not necessarily where the
Steinmetz fit itself was characterized (real datasheets are often fit
from much smaller AC swings than Bsat). A flux swing can still be
*below* Bsat and *outside* the coefficient's real, uncharacterized
fitted range — one such case was found during the same live check
(N88 at 300 kHz, swing 0.39 T against a 0.51 T Bsat) still reporting a
implausibly large core-loss figure. Closing this fully needs the real
`minFluxSwingT`/`maxFluxSwingT` data this database doesn't have yet —
not invented here, consistent with every other honest gap in this
document.

Core loss also carries detail fields surfacing what was already computed
internally but never exposed: `coreLossMaterialUsed`,
`coreLossCoefficientMinFreqHz`/`MaxFreqHz` (the matched row's own declared
valid range, not the requested frequency), `coreLossFluxDensitySwingT`,
`coreLossVolumeM3`, `coreLossDensityWPerM3`.

**Flux-swing valid-range guard:** the coefficient row optionally carries
`minFluxSwingT`/`maxFluxSwingT` — when both are present, a computed swing
outside that range is rejected (`not_evaluated`, "never extrapolated
silently"), mirroring the frequency-range guard above. Currently a
documented no-op: `data/real_core_loss_coefficients.csv` carries no such
columns, so these are always unset against the current snapshot.

### Known remaining gap

The coefficient rows also carry temperature-correction terms
(`ct0`/`ct1`/`ct2`), which are **not yet applied** — their exact formula
wasn't confirmed against the upstream source, and applying a guessed
correction would be worse than not applying one at all. Core loss today
is computed at the coefficient's fitted reference temperature, not
corrected for `ambientTemperatureC`.

---

## 10. Thermal Evaluation (Real Iterative Loop)

**File:** `src/core/thermal/ThermalEvaluation.cpp`

### What it's for

Predicts winding temperature rise from the losses computed above, feeding
back into a temperature-corrected ("hot") DCR and copper loss — closing
the loop between heat and resistance, since hotter copper has higher
resistance, which dissipates more power, which raises temperature further.

### Why an iterative loop

Copper resistance rises with temperature (`copperTempCoefficientPerC`,
0.00393/°C — a real IACS physical constant), so hot DCR depends on the very
temperature it's used to predict. This is solved the same way turns/gap
(Section 4) is: seed, compute, check for stability, repeat.

```
thermalResistanceCPerWUsed = estimateThermalResistanceCPerW(aeMm2, leMm)  // see below, computed once
windingTempC = ambientTemperatureC
repeat:
  hotDcrOhms  = coldDcrOhmsAt20C * (1 + copperTempCoefficientPerC * (windingTempC - 20))
  copperLossW = rmsCurrentA^2 * hotDcrOhms
  knownLossW  = copperLossW + (coreLossW if known)
  riseC       = knownLossW * thermalResistanceCPerWUsed
  newWindingTempC = ambientTemperatureC + riseC
until |newWindingTempC - windingTempC| < thermalConvergenceThresholdC (or maxThermalIterations reached)
```

**Thermal resistance is now size-aware**, not one flat number for every
core. `estimateThermalResistanceCPerW()` (`ThermalEvaluation.cpp`) uses
Newton's law of cooling (`Rth = 1 / (h * surfaceAreaM2)`) over a surface
area estimated from the candidate's own real magnetic-circuit geometry:

```
volumeM3      = aeMm2 * leMm * 1e-9
surfaceAreaM2 = compactSolidSurfaceAreaShapeFactor * volumeM3^(2/3)   // cube approximation, factor=6
thermalResistanceCPerWUsed = 1 / (naturalConvectionCoefficientWPerM2K * surfaceAreaM2)
```

`naturalConvectionCoefficientWPerM2K` (10 W/(m²·K)) is a real, citable
natural-convection-in-still-air value (standard heat-transfer references
put the range at ~5–25 W/(m²·K)); `compactSolidSurfaceAreaShapeFactor`
(6, the real cube surface-to-volume relation) is a documented
order-of-magnitude simplification that does not yet differentiate real
shape families (a toroid's real surface-to-volume ratio is higher than a
cube's, so this under-states — i.e. is conservative about — a toroid's
real surface area). When a candidate's `aeMm2`/`leMm` are unavailable, the
loop falls back to the flat `defaultThermalResistanceCPerW` (15°C/W)
constant this replaced. Either way, this is still an estimate, never
per-core measured or simulated Rth data, so this loop can only ever
produce a `PreliminaryThermalEstimate`, never a "fully evaluated" thermal
result — `ThermalStatus` has no such value at all. `ThermalValidation`
still runs a real pass/fail comparison against `allowableTempRiseC` on a
preliminary result, flagged via `ValidationResult.isPreliminaryEstimate:
true`.

### Genuine non-convergence (positive-feedback divergence)

Because hotter winding → higher DCR → more loss → hotter winding is a real
positive-feedback loop, its local iteration gain
(`rmsCurrentA^2 * coldDcrOhmsAt20C * copperTempCoefficientPerC * thermalResistanceCPerWUsed`)
can reach or exceed 1 for a high-current, low-DCR design — in which case
the loop genuinely diverges rather than converges (confirmed against an
independent hand-simulation of the identical formula before writing
`tests/cpp/ThermalTests.cpp`'s divergence test). A non-converged result
reports `not_evaluated`, never a stale intermediate number. Because the
gain scales with `thermalResistanceCPerWUsed`, moving from the flat 15°C/W
constant to a real per-core estimate can change whether a *specific*
candidate's loop is even stable — a large core with a real, low geometry-
derived Rth is *less* likely to diverge than the flat constant implied,
which can surface a genuine, previously-hidden thermal failure that used
to sit silently at `NotEvaluated` (see the case-6 golden reference test
for a documented real example: E100/60/28-3C90 at 28A RMS).

### Feeds into

`WindingDesignResult.estimatedHotDcrOhms` and `LossEvaluationResult.copperLossW`
are overwritten with the converged values whenever the loop converges
(`InductorDesignService.cpp`) — otherwise the cold-reference/sanity-check
values from Sections 6/8 remain, honestly.

---

## 11. Skin-Depth AC-Loss Risk (Qualitative Heuristic)

**File:** `src/core/losses/SkinDepthRisk.h`/`.cpp` (renamed from the dead
`HighFrequencyLosses.{h,cpp}` stub, which always returned 0.0 and was never
even called)

### What it's for

Flags when a selected conductor's diameter is large enough, relative to
the classical skin depth at the switching frequency, that real AC
(skin-effect) resistance is likely significantly higher than the DC value
this engine computes everywhere else. **Qualitative only** — this is a
risk *level*, never a watts figure.

```
skinDepthM = sqrt(rho_Cu / (pi * f * mu0))
ratio      = strandRadiusMm / skinDepthMm
riskLevel  = Low       if ratio <= skinDepthRiskModerateThreshold (1.0)
             Moderate  if ratio <= skinDepthRiskHighThreshold (2.0)
             High      otherwise
```

`rho_Cu` (1.724e-8 Ω·m) and `mu0` (4π×10⁻⁷ H/m) are real physical
constants; the two threshold ratios are Phase 1 heuristic boundaries, not
a validated Dowell/FEA AC-loss limit. `acLossWattsStatus` is permanently
`not_evaluated` — no AC-loss watts model exists. The result's `reason`
names explicitly what *was* evaluated (single-strand skin effect) and what
was **not** (proximity effect between bundled parallel strands, proximity
to the air gap) — no winding-layer or winding-to-gap geometry exists
anywhere in this engine to compute either from.

---

## 12. Recommendation Tiers and Loss Naming

**Files:** `src/validation/RecommendationStatus.cpp`, `src/backend/services/InductorDesignService.cpp`

Not a formula, but the piece that ties every result above into a single
honest verdict. `determineRecommendationStatus()` replaces a binary pass/fail with
three tiers:

```
Rejected             = passed == false (mirrors the existing check aggregation, never overridden)
ConditionalPass = passed == true, but any check not_evaluated, any check isPreliminaryEstimate,
                        or AC-loss risk is Moderate/High
Pass    = passed == true and none of the above
```

**Currently unreachable in practice:** `ThermalValidation` (Section 10)
sets `isPreliminaryEstimate: true` on every result it ever produces, so no
real Phase 1 request reaches `Pass` today — confirmed against
17 real passing candidates from a live request, all `ConditionalPass`.
The tier exists for when real thermal-resistance and AC-loss-watts data
eventually exist, not as a claim that any design today is unconditionally
ready to build.

Loss naming: `LossSummary.knownPartialLossW` sums whichever of copper/core
loss are `Evaluated` — labeled "Known Partial Loss," never "Total Loss."
`isCompleteTotal` is permanently `false`, since AC-loss watts (Section 11)
never becomes `Evaluated`.

Ranking (`InductorDesignService.cpp`'s `candidateRanksAhead()`) sorts by
tier first, then known evaluated loss, predicted temperature rise,
manufacturability margin (a documented composite of physical-fill headroom
and the small-gap warning penalty), saturation margin, current-density
margin, area product, and finally part number as a deterministic tiebreak.
A missing number in any tiebreaker ranks as the worst case for that
dimension, never the best, so a gap in the data can never accidentally win
a ranking.

---

## Parameter Glossary (Quick Reference)

| Symbol | Full name | Unit | Appears in |
|---|---|---|---|
| `L` | Inductance | H (henries) | Sections 2, 4, 5 |
| `Ipk` | Peak current | A | Sections 2, 5 |
| `Irms` | RMS current | A | Sections 1, 6, 8 |
| `Iavg` | Average current | A | Section 1 |
| `N` | Turns count | turns | Sections 4, 5, 6 |
| `Ae` | Core effective area | mm² (or cm²/m² after conversion) | Sections 3, 4, 5 |
| `Wa` | Core window area | mm² | Sections 3, 6 |
| `Le` | Core magnetic path length | mm (or cm after conversion) | Section 4 |
| `muR` | Relative permeability | dimensionless | Section 4 |
| `AL` | Inductance factor | nH/turn² | Section 4 |
| `gap` | Air gap length | cm/mm | Section 4 |
| `Ap` | Area product | cm⁴ | Sections 2, 3 |
| `Bpk` | Peak flux density | T | Section 5 |
| `Bmax` / `Blimit` | Flux density limit | T | Sections 2, 5, 7 |
| `Ku` | Window utilization factor | dimensionless (0-1) | Section 2 |
| `J` | Current density | A/cm² or A/mm² | Sections 2, 6 |
| `DCR` | Winding DC resistance | ohms | Sections 6, 8 |
| `MLT` | Mean length per turn | mm | Real-geometry estimate for most cores — see Section 6 |
| `Pcu` | DC copper loss | W | Section 8 |
| `Pv` | Core loss density | W/m³ (see Section 9's units caveat) | Section 9 |
| `fillFactor` | Fraction of window filled with copper | dimensionless (0-1) | Section 6 |

---

## Where the Numbers Come From, Summarized

| Category | Examples | Source |
|---|---|---|
| Direct user input | `L`, `Ipk`, `Irms`, switching frequency, temperatures | The API request (`InductorDesignRequest`) |
| Real manufacturer data | `Ae`, `Wa`, `Le`, `MLT`, `muR`, `AL`, `BmaxT`, material frequency ranges, Steinmetz coefficients | `data/real_cores.csv`, `data/real_materials.csv`, `data/real_core_loss_coefficients.csv` |
| Engineering policy defaults | `Ku`, `Bmax` default, `J`, saturation margin, fill factor limit, tolerance, min single-strand AWG | `DesignRules::phase1Default()` — one named place, never hidden in a route handler |
| Physics/unit constants | `0.4*pi`, cm-to-mm conversions, `0.5` in the energy formula | Fixed, not configurable — they're not design choices |
| Computed/derived | `Ap`, `Bpk`, `gap`, `AL_eff`, `fillFactor`, `Pcu` | Calculated by the engine at request time, never stored |
