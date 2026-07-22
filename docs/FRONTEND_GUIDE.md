# Web UI Guide

This guide explains how to use the AIMagnetics web interface and what each input/output means.

---

## The Interface

Open **http://127.0.0.1:8000** (see [GETTING_STARTED.md](GETTING_STARTED.md) for how to start the server). You'll see a two-column layout: input form on the left, results on the right.

---

## Natural-Language Input ("Describe It In Plain English") — the primary path

This is the first thing on the page, above the form. Type a sentence like
*"I need a 470 uH inductor, 1.5 A peak, 1 A RMS, 0.3 A ripple, switching at
80 kHz"* and click **Fill Form From Description** — the local LM Studio
model (schema-constrained JSON output) extracts the fields, and a
**"What I understood — please verify"** card appears with the parsed values
in plain labeled units. That card, not the form, is the confirmation step:
check it at a glance, then click Generate. Missing information comes back
as clarifying questions instead of a guess (each with the reason it
matters — e.g. *"what ripple did you assume?"* because core loss needs it),
and impossible combinations (RMS > peak) are flagged as errors.

Design principles behind it (see `knowledge/input-interview-guide.md`):
the model can only translate the sentence into fields — the JSON schema
prevents prose or invented fields, an unstated quantity is `null` (never a
typical-value guess), and every sanity check and question after extraction is
deterministic Python, not AI. If RMS wasn't stated but average current and
ripple were, RMS is derived with the documented triangular-ripple formula and
shown *visibly* in the verify card for confirmation like any other value.

**Requires LM Studio running locally** (`127.0.0.1:1234`, a chat model
loaded) — on the hosted Vercel site (or if LM Studio isn't running) the
button returns an explanatory error instead of a form fill, since that
server can't reach your machine.

---

## Manual Entry — fields, edit values directly, or fully skip natural language

Collapsed by default under a **"Manual Entry / Edit Values"** disclosure,
right below the description box — click it open to type numbers directly
instead of describing them, or to correct one field the AI got wrong
without re-describing the whole thing. This is the same real form as
before; the natural-language box just fills it, it doesn't replace it. The
fields below apply regardless of which converter topology the inductor sits
in (buck, boost, a flyback's output inductor, etc.) — the tool takes the
inductor's own operating point directly, not the upstream converter spec
(Buck/Boost requirement derivation is a later phase).

### 1. Inductance (µH)
**What it is:** The desired inductance value for the inductor.
**Example:** `250`
**Why it matters:** Energy stored: `E = 0.5 × L × I²`. Too small → current ripple too high; too large → oversized/expensive.

### 2. Peak Current (A)
**What it is:** Maximum current through the inductor during operation.
**Example:** `2.0`
**Why it matters:** Drives core size (Ap requirement), peak flux density, and saturation margin. Never used to infer RMS current.

### 3. RMS Current (A)
**What it is:** The RMS current through the inductor — a separate quantity from peak current.
**Example:** `1.4`
**Why it matters:** Drives winding sizing (fill factor, current density) and DC copper loss. The Manual Entry form has no separate average-current input, so it always requires RMS directly. The natural-language box can derive it for you: if your description states average current and peak-to-peak ripple instead of RMS, the derivation (`Irms = sqrt(Iavg^2 + ripple^2/12)`, triangular ripple) happens automatically and the result is shown in the verify card and filled here for confirmation.

### 4. Switching Frequency (kHz)
**What it is:** How fast the circuit switches (PWM frequency).
**Example:** `100`
**Why it matters:** Determines which materials are frequency-compatible candidates.

### 5. Ambient Temperature (°C)
**What it is:** The environment temperature the inductor operates in.
**Example:** `25`
**Current status:** accepted and threaded through to `ThermalEvaluation`, which always reports `not_evaluated` today — no thermal-resistance model or data exists in either CSV yet.

### 6. Allowable Temperature Rise (°C)
**What it is:** How much hotter than ambient the core can get.
**Example:** `40`
**Current status:** a real `ThermalValidation` check exists and runs, but always reports `not_evaluated` (never assumes a pass) pending the same thermal data gap.

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

## Design Summary (Results) — what's actually shown

### Active Rules & Assumptions panel
The full `DesignRules::phase1Default()` ruleset used for this run (Ku, Bmax default, current density, saturation margin, fill factor, inductance tolerance) — never a hidden constant.

### Feasibility panel
Either `status: "ok"` with a summary message, or `status: "no_feasible_design"` with the reason and, for an area-product shortfall, the required vs. largest-available area product.

### Triage panel
The first thing to look at when candidates come back. Shows how many candidates were evaluated, how many passed, and how many were rejected, plus a tally of *why* — every rejected candidate's failed check names, counted and sorted (e.g. `PeakFluxValidation × 19`). This exists specifically so a batch of mostly-rejected results (the common case with the current Ferroxcube-only core snapshot) is scannable in one glance instead of requiring every candidate to be opened individually.

### Candidates table
One row per evaluated candidate — passing and rejected together, filterable to All / Passing / Rejected and sortable by clicking any column header (status, core, material, turns, gap, calculated inductance, error %, fill factor, copper loss, core loss, total loss). Any field the engine reports `not_evaluated` for (DC copper loss, core loss) shows as an amber "not evaluated" chip in the table rather than a blank or a fabricated 0. The **Total Loss** column and the note above the table make the ranking policy visible directly in the UI: passing candidates are sorted by total loss (Cu + Core, whichever are evaluated) ascending, falling back to smallest area product only when a candidate has no loss data at all. Clicking a row expands it in place to show winding detail, core loss, every validation check (`Validation checks`, expandable), rejection reasons, and missing-data warnings — the same information the old per-candidate cards showed, just collapsed by default so 20+ rejected candidates don't flood the page.

### Design Summary panel
Recommended material, core, turns/gap, and total loss (Cu + Core) for the top-ranked passing candidate, plus a one-line note on why it ranked #1 (lowest total loss, or smallest area product if no candidate had loss data) — or a note that nothing passed.

---

## Troubleshooting Results

### `status: "no_feasible_design"`
This is a real, structured result now, not a console-only warning — check the feasibility panel for the reason (e.g. `requiredAreaProductCm4` vs. `largestAvailableAreaProductCm4` for an area-product shortfall) and the candidates table (filtered to Rejected) for per-check failures.

### A candidate's winding/loss fields say "not evaluated"
Core loss is now real (`Pv = k*f^alpha*B^beta`) when the material has Steinmetz coefficients in `data/real_core_loss_coefficients.csv` (17 of 32 materials) AND the request supplied `rippleCurrentPeakToPeakA` — `not_evaluated` for the rest, since flux-density swing can only be computed from real ripple current, never approximated from peak flux. The form now has an optional **Ripple Current p-p (A)** field for exactly this — leave it blank and core loss honestly reports `not_evaluated`; fill it and core loss (and total-loss ranking) become real. DC copper loss and DCR are now real for cores with a usable mean-length-per-turn estimate (`data/real_cores.csv`'s `Mlt` column) — `not_evaluated` only for the subset of cores whose upstream geometry doesn't support that estimate. High-frequency loss and thermal rise remain genuinely unimplemented. See [DATA_FILES.md](DATA_FILES.md) — these are real data gaps, not silent bugs, and the tool says so explicitly via `missingData`/`missingDataWarnings`, surfaced in the UI as amber "not evaluated" chips rather than blank cells or fake zeros.

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