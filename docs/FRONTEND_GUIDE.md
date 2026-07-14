# Web UI Guide

This guide explains how to use the AIMagnetics web interface and what each input/output means.

---

## The Interface

Open **http://127.0.0.1:8000** (see [GETTING_STARTED.md](GETTING_STARTED.md) for how to start the server). You'll see a two-column layout: input form on the left, results on the right.

---

## Input Fields

The four inputs below apply regardless of which converter topology the inductor sits in (buck, boost, a flyback's output inductor, etc.) — the tool takes the inductor's own operating point directly, not the upstream converter spec.

### 1. Inductance (µH)
**What it is:** The desired inductance value for the inductor.
**Example:** `250`
**How to find it:** Typically already determined by your converter's circuit design (e.g. from a ripple-current calculation). Typical range: 1–10,000 µH.
**Why it matters:** Energy stored: `E = 0.5 × L × I²`. Too small → current ripple too high; too large → oversized/expensive.

### 2. Peak Current (A)
**What it is:** Maximum current through the inductor during operation.
**Example:** `2.0`
**Why it matters:** Drives core size (higher current → larger Ap requirement), wire heating, and saturation risk.

### 3. Switching Frequency (kHz)
**What it is:** How fast the circuit switches (PWM frequency).
**Example:** `100`
**Why it matters:** Different materials are optimal at different frequencies (low freq → Powder Iron, high freq → Ferrite). This is the only input currently used by material selection.

### 4. Temperature Rise (°C)
**What it is:** How much hotter than ambient the core can get.
**Example:** `40`
**Current status:** accepted as input and echoed back in the "Design Details" panel, but **not yet checked against anything** — the thermal validation that would compare this limit to predicted losses is part of Stage 4, which isn't implemented yet.

---

## Generate Recommendation

Clicking **"Generate Recommendation"** calls three endpoints in sequence:
1. `POST /material-selection`
2. `POST /calculate` (area product)
3. `POST /core-selection`

**Not yet part of this flow:** turns calculation, wire gauge, copper/core loss, or a real temperature-rise check. The result panel's own "Next Step" field says **"Turns & Loss Design"** — the UI already tells you this is pending.

---

## Design Summary (Results) — what's actually shown today

### Material Recommendation panel
- Recommended material (e.g. "Kool Mu")
- Reference µ (µ_opt)
- Reason text from `materials.csv`
- Alternatives — shown as the raw pipe-delimited string from the database (e.g. `Ferrite|Powder Iron`), not a formatted list

### Core Recommendation panel
- Part number
- Material
- µ, AL (nH/T²)
- Ae (mm²), Wa (mm²), Le (mm)

### Design Details panel
- Stored Energy (mJ)
- Area Product / Ap (cm⁴)
- Allowed Temp Rise — just your input value, echoed back (not yet a pass/fail check)

### Design Summary panel
- Recommended Material
- Recommended Core
- Next Step: **"Turns & Loss Design"** (a placeholder acknowledging Stage 4 isn't built)

### Not currently shown (planned, not implemented):
- Calculated turns count
- Wire gauge
- Copper loss / core loss / total loss
- Predicted temperature rise with pass/fail status

---

## Troubleshooting Results

### "No cores meet the Ap requirement" — doesn't actually happen as an error
If no core in the database satisfies the Ap requirement, the tool doesn't return an error — it silently falls back to the largest available core in `cores.csv` and logs a warning to the server console (not visible in the browser). If your selected core looks oversized for the input, check the terminal output.

### Material seems wrong for the frequency
Check `data/materials.csv`'s `MinFrequencyHz`/`MaxFrequencyHz` — the tool returns the *first* material in file order whose range contains your frequency.

### Alternatives field shows a raw string with a `|` in it
This is expected right now — the API returns `alternatives` as a single pipe-delimited string, and the frontend doesn't currently split it into a list.

---

## Typical Design Scenarios (inputs only — outputs limited to material + core today)

### Low-Power Example
L = 10 µH, Ipk = 1.5 A, f = 500 kHz, ΔT = 40°C → expect a small, high-frequency-rated core (Ferrite or Kool Mu)

### Mid-Power Example
L = 250 µH, Ipk = 3 A, f = 100 kHz, ΔT = 40°C → expect a mid-size Kool Mu core

### High-Power Example
L = 500 µH, Ipk = 7 A, f = 50 kHz, ΔT = 50°C → expect a larger Kool Mu or Powder Iron core

*(Turns and loss figures for these scenarios can't be generated yet — see Stage 4 status in [WORKFLOW.md](WORKFLOW.md).)*