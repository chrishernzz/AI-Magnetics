# Web UI Guide

This guide explains how to use the AIMagnetics web interface and what each input/output means.

---

## The Interface

Open **http://127.0.0.1:8000** (see [GETTING_STARTED.md](GETTING_STARTED.md) for how to start the server). A slim, sticky header (`AIMagnetics` on the left) stays pinned at the top - it's a tool header, not a marketing banner, so it stays out of the way of the actual work. The header's right side carries a small white AMETEK badge (`frontend/assets/ametek-logo.png`, served at `/static/assets/ametek-logo.png`) - the logo is a solid-white asset with no transparency, so it sits in its own white chip rather than directly on the header's dark background. Below the header is a two-column layout: input form on the left, live Diagnostics on the right. There's no separate results/summary card up top - once a recommendation is generated, the top-ranked candidate is the row badged **Recommended** in the Candidates table below, so the same numbers aren't shown twice in two different places on the page. The Diagnostics card stays in view (sticky, below the header) as you scroll the input form.

Every card leads with a small icon badge, title, and one-line subtitle (e.g. Diagnostics / "Recalculated live, before you commit") instead of a bare heading - each card reads at a glance without having to read its body first.

A few small, deliberately restrained animations give feedback without being distracting: switching between Mode 1/Mode 2 fades the newly-shown section in, a live Diagnostics value briefly highlights when it actually changes (not on every keystroke - only when the number itself moves), and expanding a candidate row settles in rather than snapping open. All of it respects `prefers-reduced-motion`.

**In Buck mode**, the Diagnostics card also draws a small real waveform - the inductor current over one switching period (a triangle wave: rises for the duty-cycle fraction of the period, falls for the rest) - directly from the same live `dutyCycle`/`peakCurrentA`/`rippleCurrentPeakToPeakA` numbers shown as text above it. It's a real inline SVG chart with real axes, not an image or a decorative sketch: a y-axis with two ticks and amplitude labels (peak/min, in amps), and an x-axis with three time ticks (`0`, `T`, `2T`, where `T` is one switching period). The axis frame itself is fixed geometry - the plot is always auto-scaled so the peak lands exactly on the top axis line and the min exactly on the bottom one (the way a real oscilloscope's autoscale works), so only the axis *labels'* text changes with magnitude, not their position. What moves is the trace itself: its shape shifts horizontally whenever duty cycle changes (e.g. changing Vout or Vin Maximum), and it's drawn by updating the same SVG elements in place rather than rebuilding the chart from scratch, so the trace, its filled area, and the peak marker morph smoothly between shapes instead of snapping. A small pulsing dot marks the current peak.

---

## Two Modes: Buck Converter, or Direct Inductor Requirements

At the top of the Design Requirements card is a mode toggle:

- **"I know my inductor requirements"** (Mode 2, default) — enter the
  inductor's own operating point directly. This is topology-agnostic:
  the same fields apply whether the inductor sits in a buck, boost, or a
  flyback's output stage.
- **"I know my Buck converter requirements"** (Mode 1) — enter the
  converter's operating point instead (input voltage range, output
  voltage, output current, switching frequency, target ripple), and the
  tool derives the inductor's requirements for you. V1 supports Buck
  only; Boost/Flyback derivation is a later phase.

Clicking **Calculate Magnetic Requirements** in Mode 1 calls
`POST /topology-design/buck`, then automatically switches to Mode 2 with
the derived values filled in, under a banner explaining exactly what was
derived and at which worst-case input voltage (`Vin_max` — see
[API_REFERENCE.md](API_REFERENCE.md) for why). This banner lives in the
Diagnostics column, below the live diagnostics panel, not inside the
Design Requirements form itself — it's a summary of what got carried
over, not another input to fill in, so it doesn't interrupt the form.
The RMS Current field is disabled in this state, since the derived
request uses average current + ripple current instead — the same
triangular-ripple RMS derivation Mode 2 already relies on when those two
fields are supplied directly. Every field is still editable after
deriving; click **"Clear and enter inductor requirements directly"** in
the banner to discard the derivation and go back to entering everything
by hand.

Both modes end at the same place: the fields below, and the same
**Generate Recommendation** button and pipeline.

Within each mode, the input fields are split into two lightweight
sub-tabs - "Buck Converter" / "Electrical" and "Thermal & Tolerance" -
so only one group is visible at a time instead of every field stacked
in one long column. This is a display-only split: every field keeps its
id and still submits normally regardless of which sub-tab is showing.
The sub-tabs sit below the primary Mode 1/Mode 2 toggle and are styled
lighter (underline, not filled pills) so the two tab levels don't
visually compete.

## Live Diagnostics (right column)

Recalculates as you type, before you've clicked anything:

- **In Buck mode**: calls `POST /topology-design/buck` on a short debounce
  (~350ms after you stop typing) and shows Duty Cycle, Ripple Current,
  Peak Current, Average Current, and the highlighted Required Inductance
  result — the exact same numbers `Calculate Magnetic Requirements` would
  commit, just visible before you commit to them. Rows show the label and
  the bold result only; the formulas themselves live in exactly one
  place — the collapsed **"How these are calculated"** dropdown at the
  bottom of the Diagnostics card, after the waveform. Earlier this also
  duplicated the same formula as a small caption under every row, which
  just added scroll height for a formula the dropdown already had -
  removed in favor of the dropdown as the single source. Expand it to see
  all four formulas plus a note that everything is sized at Vin Maximum,
  not Vin Minimum. It doesn't take up form space until opened, the same
  collapsed-by-default pattern as the Active Rules & Assumptions strip
  below the results. The waveform chart below the rows has its own
  one-line caption explaining what it's showing. If the current values
  aren't physically valid yet (e.g. Vout ≥ Vin Maximum while you're still
  typing), the row values grey out and a note explains why, rather than
  showing a hard error on every keystroke.
- **In Direct mode**: a client-side sanity panel — Stored Energy
  (`E = 0.5 × L × I²`, the same formula shown in the Inductance field's
  hint), the RMS-vs-peak warning, and a live note on whether core loss
  will be evaluated based on whether Ripple Current is filled in.

## Input Fields (Mode 2 - Direct Inductor Requirements)

The inputs below apply regardless of which converter topology the inductor sits
in (buck, boost, a flyback's output inductor, etc.) — the tool takes the
inductor's own operating point directly, not the upstream converter spec.
If you'd rather start from your Buck converter's operating point, use Mode 1
above instead - it fills these in for you.

### 1. Inductance (µH)
**What it is:** The desired inductance value for the inductor.
**Example:** `250`
**Why it matters:** Energy stored: `E = 0.5 × L × I²`. Too small → current ripple too high; too large → oversized/expensive.

### 2. Peak Current (A, optional)
**What it is:** Maximum current through the inductor during operation, if known.
**Example:** `2.0`, or leave blank.
**Why it matters:** Drives core size (the area-product pre-filter), peak flux density, and saturation margin - never inferred from RMS current, which would understate the real peak and silently undersize the core. Leave it blank and the area-product pre-filter is skipped (every frequency/material-compatible core is evaluated directly instead), and `PeakFluxValidation`/`SaturationValidation` report `not_evaluated` per candidate rather than guessing.

### 3. RMS Current (A)
**What it is:** The RMS current through the inductor — a separate quantity from peak current.
**Example:** `1.4`
**Why it matters:** Drives winding sizing (fill factor, current density) and DC copper loss. If you only know average current and peak-to-peak ripple, the API can derive this for a triangular ripple waveform (`Irms = sqrt(Iavg^2 + ripple^2/12)`) — the web UI requires you to enter RMS current directly.

### 4. Switching Frequency (kHz)
**What it is:** How fast the circuit switches (PWM frequency).
**Example:** `100`
**Why it matters:** Determines which materials are frequency-compatible candidates.

### 5. Ambient Temperature (°C)
**What it is:** The environment temperature the inductor operates in.
**Example:** `25`
**Current status:** accepted and threaded through to `ThermalEvaluation`, which runs a real iterative convergence loop (temp → hot DCR → copper loss → temp rise → repeat) using a size-aware Rth estimated from each candidate's own real core geometry (Newton's law of cooling; falls back to a flat Phase 1 default only when geometry is unavailable) and caps at `PreliminaryThermalEstimate` either way, never per-core measured data; reports `not_evaluated` only when DCR geometry is unknown or the loop diverges.

### 6. Allowable Temperature Rise (°C)
**What it is:** How much hotter than ambient the core can get.
**Example:** `40`
**Current status:** a real `ThermalValidation` check exists and runs, comparing the converged predicted temp rise against this limit and flagging the result `isPreliminaryEstimate: true`; reports `not_evaluated` only when the thermal loop itself can't produce a result.

### 7. Ripple Current p-p (A, optional)
**What it is:** The peak-to-peak ripple current the converter design assumes.
**Example:** `0.3`
**Why it matters:** Core loss depends on the flux-density swing the ripple causes (`ΔB = L·ΔI/(N·Ae)`) — with this field blank, core loss reports `not_evaluated` for every candidate rather than guessing, and ranking falls back to copper loss alone.

### 8. Inductance Tolerance (%, optional)
**What it is:** How far the realized inductance may deviate from the target before a candidate is rejected.
**Default:** 10% (`DesignRules.defaultInductanceTolerancePercent`) if left blank.

---

## Generate Recommendation

Clicking **"Generate Recommendation"** calls a single endpoint: `POST /inductor-design`. This runs the entire Phase 1 pipeline once — material candidates, area product, core candidates, turns/gap design, magnetic validation, winding design, and loss evaluation — and returns one explainable result. The old four-endpoint sequential-fetch flow is gone; there's nothing further to trigger.

---

## Results — what's actually shown

### Active Rules & Assumptions panel
The full `DesignRules::phase1Default()` ruleset used for this run (Ku, Bmax default, current density, saturation margin, fill factor, inductance tolerance) — never a hidden constant.

### Feasibility panel
Only rendered for `status: "no_feasible_design"` — the reason, and for an area-product shortfall, the required vs. largest-available area product. When `status: "ok"`, this panel doesn't render at all: the Candidates triage panel immediately below already shows the pass/reject counts, so a second box repeating the same sentence was removed rather than kept as a duplicate.

### Triage panel
The first thing to look at when candidates come back. Shows how many candidates were evaluated, how many passed, and how many were rejected. Rejection detail (each failed check, its real reason) lives once, in each rejected candidate's own side panel — not repeated here as a summarized tally.

### Recommended Candidate card
A dedicated card above the table surfacing `result.candidates[0]` — already the backend's own top-ranked candidate (`candidateRanksAhead()` in `InductorDesignService.cpp`), not a second calculation of anything. Shows the part number, shape, tier chip, and a **completeness chip** (`N/M evaluated` — counts only `ValidationResult.mandatory` checks, so it can never disagree with the tier sitting next to it), a KPI strip, a real "why this one" paragraph (`recommendation.explanation`, the same sentence a click-through to the side panel would show), a "still not evaluated" list naming exactly which mandatory checks didn't run and why, and a "next steps" line. Hidden entirely when no candidate passed (the empty state below explains why instead). Clicking Generate scrolls the page straight to this card (`scrollToResultsCard()` in `app.js`, offset for the sticky top header's real height so the card's own title is never clipped behind it) — falls back to scrolling to the candidates table itself when there's no passing candidate to recommend.

### Candidates table
Passing and Rejected are real tabs (`switchCandidatesTab()` in `app.js`), not a stacked accordion — one panel visible at a time, switching is a single click on a tab that never moves, rather than scrolling to find and click a `<summary>` bar. The Rejected tab is hidden entirely when there are no rejected candidates this run. Every fresh run resets back to the Passing tab. Two filter dropdowns sit next to the card title and apply to both tabs: a **shape filter** (All shapes / Toroid / Two-Piece Set, added after a real user report that the tool had no way to search by core shape) and a **material-type filter** (All types / Ferrite / Powder, added once candidate counts grew into the hundreds and ferrite started crowding out powder in a single ranked list — see `docs/DATA_FILES.md` for why the real core catalog grew). Each core's real shape gets its own **Shape** column, and its real material grade its own **Material** column; hover the shape chip to see the geometry family, e.g. "T", "ETD", "PQ". There's no per-column sort control — the tool always names one specific recommended candidate, and re-sorting by another column can't change which one that is. Any field the engine reports `not_evaluated` for (DC copper loss, core loss) shows as muted inline text rather than a blank or a fabricated 0; a toroid's gap shows "Distributed gap" rather than a misleading `0.00 mm` (see `TurnsAndGapDesign.cpp`'s `isDistributedGapCore` branch). Each row also carries its own completeness chip.

Before any run, and when a filter matches nothing, the table area shows a distinct empty state (never a blank `<table>`) — a first-look "enter your requirements and generate" message, a "no passing candidate matched this filter" message, or (while a request is in flight) a shimmering loading skeleton.

Clicking a row opens its full detail in a **slide-in side panel** from the right edge of the screen. The table stays visible and scrollable behind the panel. Close it with the × button, by clicking the dimmed overlay, or with Escape. Clicking a different row while the panel is open just re-renders its contents rather than stacking a second panel.

**Candidate detail panel layout**: the panel header repeats the core part number, its real shape, material, and pass/reject state. The body leads with an always-visible **Overview** — a 5-box KPI strip (Known Partial Loss, Core Loss, Physical Fill %, Current Density, Turns/Gap) plus a one-line status carrying the completeness chip and tier — the numbers and outcome a candidate is actually judged by, never collapsed. Below that, **Validation Checks** (open by default), **Sources**, and **Missing-data warnings** are each their own `<details class="detail-group">` group, wired as a real accordion (`wireDetailGroupAccordion()` in `app.js`) - opening one closes the others, so the panel never has more than one section expanded fighting for space at once. Nothing about any check is ever stated in two different places — there's exactly one row per check, and that row is the only place its numbers and its reason live.

### Input validation
A real client-side hard block, not a soft warning next to an enabled button: `validateDirectInputs()` checks the same physical relationships the backend's `RequirementDerivationService::derive()` and `CurrentConsistencyValidation` would otherwise only catch after a full round trip (RMS exceeding peak, ripple implying a negative minimum inductor current). While any error is present, an `.input-validation-card` lists every fix needed and the Generate button is disabled (`:disabled` gets its own dimmed style, not just a functional no-op) — checked live on every keystroke, and defensively re-checked inside `generateRecommendation()` itself regardless of what triggered it.

---

## Troubleshooting Results

### `status: "no_feasible_design"`
This is a real, structured result now, not a console-only warning — check the feasibility panel for the reason (e.g. `requiredAreaProductCm4` vs. `largestAvailableAreaProductCm4` for an area-product shortfall) and the candidates table (filtered to Rejected) for per-check failures.

### A candidate's winding/loss fields say "not evaluated"
Core loss is now real (`Pv = k*f^alpha*B^beta`) when the material has Steinmetz coefficients in `data/real_core_loss_coefficients.csv` (29 of 165 materials) AND the request supplied `rippleCurrentPeakToPeakA` — `not_evaluated` for the rest, since flux-density swing can only be computed from real ripple current, never approximated from peak flux. The form now has an optional **Ripple Current p-p (A)** field for exactly this — leave it blank and core loss honestly reports `not_evaluated`; fill it and core loss (and total-loss ranking) become real. DC copper loss and DCR are now real for cores with a usable mean-length-per-turn estimate (`data/real_cores.csv`'s `Mlt` column) — `not_evaluated` only for the subset of cores whose upstream geometry doesn't support that estimate. High-frequency loss and thermal rise remain genuinely unimplemented. See [DATA_FILES.md](DATA_FILES.md) — these are real data gaps, not silent bugs, and the tool says so explicitly via `missingData`/`missingDataWarnings`, surfaced in the UI as amber "not evaluated" chips rather than blank cells or fake zeros.

### RMS current shows a warning under the input
The form flags (but does not block) RMS current entered higher than peak current — physically that would mean the waveform's average heating effect exceeds its own instantaneous maximum, which cannot happen. It's a sanity check on typos, not a hard validation rule.

---

## Typical Design Scenarios

### Low-Power Example
L = 10 µH, Ipk = 1.5 A, Irms = 1.0 A, f = 500 kHz, Tambient = 25°C, ΔT = 40°C → expect a small, high-frequency-rated ferrite core

### Mid-Power Example
L = 250 µH, Ipk = 3 A, Irms = 2.1 A, f = 100 kHz, Tambient = 25°C, ΔT = 40°C → expect a mid-size ferrite core with turns/gap computed

### High-Power Example
L = 500 µH, Ipk = 7 A, Irms = 4.9 A, f = 50 kHz, Tambient = 25°C, ΔT = 50°C → check whether the bundled Ferroxcube-only snapshot has a large enough core, or expect `no_feasible_design`