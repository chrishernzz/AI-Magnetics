# Input Interview Guide

This note exists for the natural-language-input feature: when an engineer
describes what they need in plain English, these are the questions to ask to
get a *complete, correct* specification — and the reasons behind each
question, so the assistant can explain *why* it's asking.

## The required inputs and their clarifying questions

### Inductance (µH)
- If stated ("450 µH"): **"What ripple current / operating point produced that
  value?"** — L is derived from a ripple choice (see [[choosing-inductance]]);
  knowing the origin lets it be sanity-checked, and the ripple is needed for
  core loss anyway.
- If not stated but Vin/Vout/Iout/f are: offer to derive L from a target
  ripple percentage instead of asking for L directly.

### Peak current (A)
- **"Is that the steady-state peak, or does it include startup/transient/
  fault current?"** — saturation is checked against the true worst case.
- If only "output current" is given: peak ≈ Iout + ΔI/2 for a buck in CCM —
  but confirm the topology before applying that.

### RMS current (A)
- Often not known directly. If average current and ripple are known:
  `Irms = sqrt(Iavg² + ΔI²/12)` (triangular ripple) — state the assumption.
- **Never** estimate RMS from peak alone; ask instead.
- Sanity: Irms > Ipk is impossible — flag it as a probable typo, don't proceed.

### Ripple current, peak-to-peak (A)
- **"What peak-to-peak ripple does the converter design assume?"** — required
  for any real core-loss number. If unknown: typical buck practice is 20–40%
  of load current; offer that as an assumption *labeled as an assumption*.

### Switching frequency (kHz)
- **"Fixed or variable? If variable, what's the range?"** — design to the
  worst corner.

### Ambient temperature and allowed rise (°C)
- **"What's the air temperature around the part, and what's the hottest the
  part is allowed to get?"** — both magnetics and insulation degrade with
  heat (see [[thermal-basics]]). Common defaults: 25 °C ambient, 40 °C rise —
  defaults are fine if labeled.

### Tolerance (%)
- **"How exact must the inductance be?"** — ±10% suits most converters;
  filters/resonant circuits may need tighter. Default 10% if unstated.

## Interview principles

1. **Ask, don't invent.** A missing number is a question, never a guess. If
   the engineer can't answer, use a documented default and *say so* in the
   result.
2. **Explain why you're asking.** "I need ripple current because core loss
   depends on the flux swing it causes" builds trust and teaches.
3. **Sanity-check silently, question loudly.** Run the cheap checks
   (Irms ≤ Ipk, ripple 20–40% of load, L consistent with f and ripple) and
   only surface the ones that fail.
4. **Confirm the parsed spec before running.** Show the engineer the complete
   structured request ("L=470 µH, Ipk=1.5 A, Irms=1.0 A, ΔI=0.3 A, f=80 kHz,
   25 °C, ΔT≤40 °C, ±10%") and get a yes before invoking the design engine.
5. **The physics engine does all math.** The assistant translates and
   interviews; it never computes design values itself.
