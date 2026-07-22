# Peak vs RMS vs Ripple Current

These are three distinct currents. Confusing them is the most common inductor
specification mistake, and each one drives a *different* part of the design.

## Definitions

- **Peak current (Ipk)** — the highest instantaneous current the inductor ever
  carries, including the top of the ripple and any transient/startup overshoot.
- **RMS current (Irms)** — the "heating equivalent" DC current. Copper loss is
  `Irms² · R`, so this is what sizes the wire.
- **Ripple current (ΔI, peak-to-peak)** — how much the current swings each
  switching cycle, top-of-triangle minus bottom-of-triangle.

## What each one drives

| Current | Drives | Because |
|---|---|---|
| Ipk | Core size (area product), peak flux density, saturation check | `B = L·Ipk/(N·Ae)` — flux is proportional to instantaneous current |
| Irms | Wire gauge, current density, DC copper loss, temperature rise | Heating is proportional to the *square* of RMS current |
| ΔI | Core loss | Core loss depends on the flux-density *swing* `ΔB = L·ΔI/(N·Ae)`, not the peak |

## Never derive one from another by guessing

- Peak current tells you **nothing** about RMS current without knowing the
  waveform. A 10 A peak could be 9.8 A RMS (nearly DC) or 3 A RMS (spiky).
- The only safe derivation is when the waveform shape is *known*. For the
  triangular ripple riding on a DC level typical of buck/boost inductors:

```
Irms = sqrt(Iavg² + ΔI²/12)
```

  where `Iavg` is the average (DC) current and `ΔI` is peak-to-peak ripple.
  This is exact for an ideal triangular waveform.

- Similarly, core loss must come from the *real* ripple current. Approximating
  the flux swing from peak flux overestimates core loss enormously (peak flux
  includes the DC component, which causes no AC loss).

## Sanity checks worth applying

- `Irms ≤ Ipk` always. RMS above peak is physically impossible — it means a
  typo.
- For a buck inductor, ripple is typically chosen as **20–40% of the load
  current**. Ripple far outside that range deserves a "are you sure?" question.
- `Ipk ≈ Iout + ΔI/2` for a buck in continuous conduction — if the stated peak
  is much higher than that, there's a transient requirement worth asking about.
