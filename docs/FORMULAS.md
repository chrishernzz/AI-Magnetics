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

**File:** `src/core/AreaProduct.cpp` — this is the **McLyman area-product
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

**File:** `src/core/CoreEvaluation.cpp`

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

**Files:** `src/core/GapDesign.cpp` (the physics), `src/core/
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
than 40% of the core's own magnetic path length) or if it doesn't converge
within 15 iterations.

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
| `Blimit` | The applicable flux density limit | T | Material-specific `BmaxT` if the material has one (none currently do), otherwise `DesignRules.defaultFluxDensityLimitT` |

**Important:** whenever `Blimit` falls back to the Phase 1 default instead
of a real material-specific number, the result explicitly flags
`usedDefaultLimit: true` — so a generic assumption is never presented as a
measured fact about a specific material.

### Feeds into

The candidate's overall pass/fail decision. A failed check here blocks the
candidate and its explanation appears in `rejectionReasons`.

---

## 6. Winding Design

**File:** `src/core/WindingDesign.cpp`, using the standard AWG (American
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

### A known limitation

Total wire length and DC resistance (DCR) are **not** computed, because
computing them needs the core's mean-length-per-turn (MLT) — a geometric
figure the current core database doesn't include. Fill factor and current
density don't need MLT (they only need turns and window area, both already
known), so they're still fully computed. DCR is reported `not_evaluated`
with an explicit explanation rather than guessed.

### Feeds into

DC copper loss (Section 8) — only when DCR was actually computed.

---

## 7. Why Bmax Defaults to 0.30 T

This isn't a separate formula, but it's a parameter that appears in
Sections 2 and 5, so it's worth explaining on its own. `DesignRules.
defaultFluxDensityLimitT = 0.30 T` is a **conservative, generic ferrite
saturation guideline** — real ferrite materials often saturate well above
this, but 0.30 T is a commonly used safe design point when a material's
actual measured saturation curve isn't available. Every material in the
current data snapshot (`data/real_materials.csv`) has `BmaxT = 0.0`
(unpopulated), so this default is what's actually used for every candidate
today. The engine never presents this as if it were a measured fact about
a specific material — every check that uses it sets `usedDefaultLimit:
true` in its result.

---

## 8. DC Copper Loss

**File:** `src/core/CopperLoss.cpp`, orchestrated by `src/core/
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
| `DCR` | Winding DC resistance | ohms | From Section 6 — **only available when MLT data exists**, which it currently doesn't |

### Why it matters if this is wrong

Using peak current here instead of RMS would overstate the loss
significantly for any real ripple waveform — this is exactly the kind of
current-field mixup the project's own rules explicitly forbid (see Section
1: peak and RMS current are never interchangeable).

### Current status

Because DCR is `not_evaluated` today (Section 6's known limitation), this
formula's real implementation exists and is tested, but it isn't currently
invoked with real numbers — `copperLossStatus` reports `not_evaluated`
rather than presenting `0 W` as if it were a real, computed answer.

---

## 9. Core Loss (Implemented, Not Currently Used With Real Data)

**File:** `src/core/CoreLoss.cpp`

### What it's for

Estimates power lost to hysteresis and eddy currents inside the core
material itself (as opposed to the winding).

### Why this formula, and its caveat

Real core-loss modeling normally uses the Steinmetz equation (`Pv = k *
f^a * B^b`, with `k`, `a`, `b` all fitted from real per-material
measurement curves). The current material database has only a single,
undocumented `CuLossFactor` placeholder column (always `0.0` for every
material), not real Steinmetz coefficients. Rather than invent exponents
that aren't backed by real data, this module implements a documented,
clearly-labeled **simplified placeholder model**:

```
Pv = CuLossFactor * (f / 100000) * (Bswing / 0.1)^2
```

| Symbol | Meaning | Unit | Source |
|---|---|---|---|
| `Pv` | Core loss density | W/cm³ | **Output** — not currently a real result, see below |
| `CuLossFactor` | Placeholder loss coefficient | — (undefined units) | Material database — **always 0.0 today** |
| `f` | Switching frequency | Hz | User input |
| `Bswing` | Flux density swing | T | Would come from Section 5's flux calculations |

### Current status

This is gated on `MaterialCandidate.hasCoreLossData`, which is **never
true** with the current data snapshot — so this formula is never actually
invoked with real numbers today. `coreLossStatus` reports `not_evaluated`.
Closing this gap means sourcing real per-material Steinmetz coefficients
from datasheets, not writing more code.

---

## 10. Not Implemented: High-Frequency (Skin/Proximity) Loss and Thermal Rise

Two results the engine could eventually produce aren't implemented at all
yet, for different reasons than the data gaps above:

- **Skin/proximity effect loss** (`src/core/HighFrequencyLosses.cpp`) —
  the AC resistance increase caused by high-frequency current crowding
  toward the outside of a conductor. No model is coded yet; this is a
  genuine scope gap, not a data gap.
- **Thermal rise** (`src/core/ThermalEvaluation.cpp`) — predicting
  temperature rise from total loss needs a thermal-resistance model (how
  effectively a given core sheds heat to ambient air), and no such model
  or supporting data exists in this project yet.

Both are real pipeline stages that run on every request and report
`not_evaluated` honestly, rather than being silently skipped.

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
| `MLT` | Mean length per turn | mm/m | Not computable today — see Section 6 |
| `Pcu` | DC copper loss | W | Section 8 |
| `Pv` | Core loss density | W/cm³ | Section 9 |
| `fillFactor` | Fraction of window filled with copper | dimensionless (0-1) | Section 6 |

---

## Where the Numbers Come From, Summarized

| Category | Examples | Source |
|---|---|---|
| Direct user input | `L`, `Ipk`, `Irms`, switching frequency, temperatures | The API request (`InductorDesignRequest`) |
| Real manufacturer data | `Ae`, `Wa`, `Le`, `muR`, `AL`, material frequency ranges | `data/real_cores.csv`, `data/real_materials.csv` |
| Engineering policy defaults | `Ku`, `Bmax` default, `J`, saturation margin, fill factor limit, tolerance, min single-strand AWG | `DesignRules::phase1Default()` — one named place, never hidden in a route handler |
| Physics/unit constants | `0.4*pi`, cm-to-mm conversions, `0.5` in the energy formula | Fixed, not configurable — they're not design choices |
| Computed/derived | `Ap`, `Bpk`, `gap`, `AL_eff`, `fillFactor`, `Pcu` | Calculated by the engine at request time, never stored |
