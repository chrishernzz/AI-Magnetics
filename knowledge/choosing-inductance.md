# Choosing the Inductance Value

The inductance value is usually *derived from the converter's requirements*,
not picked from thin air. When an engineer states "I need 450 µH," the value
encodes decisions about ripple, frequency, and load that are worth surfacing.

## Where the number comes from (buck example)

For a buck converter in continuous conduction:

```
L = Vout · (1 - D) / (ΔI · fsw)
```

- `D` — duty cycle (Vout/Vin for an ideal buck)
- `ΔI` — chosen peak-to-peak ripple current (A)
- `fsw` — switching frequency (Hz)

The designer first *chooses* an acceptable ripple (commonly 20–40% of load
current), then computes L. So the inductance value is downstream of a ripple
decision — if the ripple assumption changes, L should change too.

## Trade-offs in both directions

**Larger L:**
- Less ripple current → lower core loss, lower output capacitor stress,
  lower peak current (helps saturation margin)
- But: physically bigger/costlier core, more turns → more copper loss,
  slower transient response (current can't change as fast)

**Smaller L:**
- Smaller part, faster transient response
- But: more ripple → more core loss, higher peak current → saturation risk,
  more output voltage ripple

## Questions worth asking when someone states an inductance

These are the clarifying questions a design assistant (or a good engineer)
should ask before accepting the number:

1. **"What topology and operating point produced this value?"** — L=450 µH at
   80 kHz implies a specific ripple; knowing Vin/Vout/Iout lets it be checked.
2. **"What ripple current did you assume?"** — this is also *required* for a
   real core-loss calculation, so it should always be captured.
3. **"Is this value critical or a starting point?"** — a ±10% tolerance
   (this project's default) is fine for most converters; some circuits
   (resonant, filters) need tighter.
4. **"What's the transient requirement?"** — a large L that meets ripple specs
   may still be too slow for a fast load step.

## Typical ranges (orientation, not rules)

- Point-of-load buck, 1–10 A, 500 kHz–2 MHz: **0.47–10 µH**
- Mid-power buck/boost, 1–5 A, 100–500 kHz: **10–100 µH**
- Lower-frequency or low-ripple designs, 50–100 kHz: **100 µH–1 mH**

A stated value far outside the range its frequency implies deserves a
follow-up question rather than silent acceptance.
