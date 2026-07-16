# Web UI Guide

This guide explains how to use the AIMagnetics web interface and what each input/output means.

---

## The Interface

Open **http://127.0.0.1:8000** (see [GETTING_STARTED.md](GETTING_STARTED.md) for how to start the server). You'll see a two-column layout: input form on the left, results on the right.

---

## Input Fields

The inputs below apply regardless of which converter topology the inductor sits in (buck, boost, a flyback's output inductor, etc.) — the tool takes the inductor's own operating point directly, not the upstream converter spec (Buck/Boost requirement derivation is a later phase).

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
**Why it matters:** Drives winding sizing (fill factor, current density) and DC copper loss. If you only know average current and peak-to-peak ripple, the API can derive this for a triangular ripple waveform (`Irms = sqrt(Iavg^2 + ripple^2/12)`) — the web UI currently requires you to enter RMS current directly.

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

### 7. Inductance Tolerance (%, optional)
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

### Passing / Rejected Candidates panels
Each candidate shows material, core, turns/gap (with calculated inductance and error %), winding (wire gauge, fill factor, current density), DC copper/core loss (or `not_evaluated`), all six validation checks (expandable), rejection reasons for anything that failed, and missing-data warnings.

### Design Summary panel
Recommended material, core, and turns/gap for the top-ranked passing candidate (or a note that nothing passed).

---

## Troubleshooting Results

### `status: "no_feasible_design"`
This is a real, structured result now, not a console-only warning — check the feasibility panel for the reason (e.g. `requiredAreaProductCm4` vs. `largestAvailableAreaProductCm4` for an area-product shortfall) and the rejected-candidates panel for per-check failures.

### A candidate's winding/loss fields say "not evaluated"
Expected for every candidate today: `data/real_cores.csv` has no mean-length-per-turn column (blocks DCR and DC copper loss), and `data/real_materials.csv`'s `BmaxT`/`CuLossFactor` are 0.0 for every material (blocks material-specific flux limits and core loss). See [DATA_FILES.md](DATA_FILES.md) — these are data gaps, not bugs, and the tool says so explicitly via `missingData`/`missingDataWarnings`.

### Alternatives field shows a raw string with a `|` in it
This is expected right now — the API returns `alternatives` as a single pipe-delimited string, and the frontend doesn't currently split it into a list.

---

## Typical Design Scenarios

### Low-Power Example
L = 10 µH, Ipk = 1.5 A, Irms = 1.0 A, f = 500 kHz, Tambient = 25°C, ΔT = 40°C → expect a small, high-frequency-rated ferrite core

### Mid-Power Example
L = 250 µH, Ipk = 3 A, Irms = 2.1 A, f = 100 kHz, Tambient = 25°C, ΔT = 40°C → expect a mid-size ferrite core with turns/gap computed

### High-Power Example
L = 500 µH, Ipk = 7 A, Irms = 4.9 A, f = 50 kHz, Tambient = 25°C, ΔT = 50°C → check whether the bundled Ferroxcube-only snapshot has a large enough core, or expect `no_feasible_design`