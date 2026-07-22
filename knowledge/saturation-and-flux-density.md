# Saturation and Flux Density

## What saturation is

A magnetic core can only carry so much flux. Past that point (saturation flux
density, `Bsat`), the material's permeability collapses toward that of air —
the inductance drops sharply, ripple current spikes, and in a converter this
usually means loud failure: current runaway, MOSFET stress, sometimes smoke.

Saturation is a *hard cliff* for ferrites and a *soft slope* for powder cores:
- **Ferrite**: inductance holds nearly constant, then falls off fast near Bsat.
  Typical Bsat for MnZn power ferrite: **0.35–0.5 T at 25 °C**, and — critically —
  **lower when hot** (can drop ~20–30% at 100 °C).
- **Powder cores** (Kool Mµ, XFlux, iron powder): permeability declines
  gradually with DC bias. They "soft-saturate," which is more forgiving but
  means the effective inductance at full load is lower than at no load.

## Computing peak flux density

For a wound core with N turns carrying peak current Ipk:

```
Bpk = L · Ipk / (N · Ae)
```

- `L` in henries, `Ipk` in amps, `Ae` (core effective cross-section) in m² →
  `Bpk` in tesla.
- This uses **peak** current, not RMS — flux follows the instantaneous current.

## Design margin

Good practice is to keep peak flux comfortably below the material's Bsat —
this project's rule set requires at least a **10% saturation margin** and
prefers each material's real measured `Bsat` over any generic default. A
generic 0.3 T limit is a safe *placeholder* for MnZn ferrite but is wrong for
powder materials (some usefully exceed 1 T, e.g. XFlux class) — using real
per-material data matters.

## Questions worth asking

1. **"What's the worst-case peak current — including startup and fault
   transients?"** — saturation is checked against the true maximum, not the
   steady-state peak.
2. **"What's the maximum operating temperature?"** — Bsat falls with
   temperature; a design fine at 25 °C can saturate at 100 °C.
3. **"Ferrite or powder?"** — determines whether saturation is a cliff to
   avoid or a gradual derating to model.
