# Inductor Design Workflow (End to End)

The complete single-winding power inductor design flow, as practiced in this
project (McLyman-style, physics-first). Each stage links to its detail note.

## The pipeline

```
1. Requirements      L, Ipk, Irms, ΔI, fsw, temperatures, tolerance
        ↓               [[input-interview-guide]] - get them right first
2. Material filter   which core materials suit this frequency
        ↓               [[core-materials]], [[switching-frequency]]
3. Area product      minimum core size from stored energy
        ↓               [[area-product-sizing]]
4. Core candidates   every library core big enough - not just one pick
        ↓
5. Turns + gap       iterate N and gap until L converges within tolerance
        ↓               [[turns-and-al-value]], [[air-gaps-and-energy-storage]]
6. Validation        peak flux, saturation margin, winding fit,
        ↓            current density, inductance error, thermal
                        [[saturation-and-flux-density]]
7. Winding design    wire gauge from RMS current, fill factor, DCR
        ↓               [[winding-design-and-awg]]
8. Loss evaluation   copper loss + core loss (+ AC effects)
        ↓               [[copper-loss]], [[core-loss-steinmetz]],
                        [[skin-and-proximity-effect]]
9. Ranking           lowest TOTAL loss wins among candidates passing
                     every check - size is a tiebreaker, not the metric
```

## Principles that make the result trustworthy

- **Every rejection has a reason.** A candidate that fails says *which* check
  failed and by how much — auditable, not oracular.
- **Missing data is reported, never papered over.** If ripple current wasn't
  given, core loss reads "not evaluated" — not a fabricated zero, not a guess.
- **No silent fallbacks.** If no core is big enough, the answer is "no
  feasible design in this library," with the required vs available numbers.
- **Constants are named and visible.** Ku=0.4, J=400 A/cm², Bmax default,
  saturation margin — shown with every result, never buried in code.
- **Units stated everywhere.** See [[units-and-pitfalls]] for why.

## What "optimization" means here

Stage 9 is deliberately simple v1 optimization: rank the survivors by
measured-physics total loss. Future versions can add cost, volume, and
multi-objective trade-offs (Pareto fronts) — but only over quantities the
physics engine can actually compute. Ranking by numbers nobody computed is
decoration, not optimization.
