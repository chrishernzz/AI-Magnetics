# Copper (Winding) Loss

## DC copper loss

The baseline winding loss — current heating the wire's resistance:

```
P_cu_dc = Irms² × R_dc        (watts)
```

Uses **RMS** current (heating equivalent), never peak. `R_dc` comes from wire
length (turns × mean-length-per-turn) and gauge (see
[[winding-design-and-awg]]).

## Temperature matters

Copper resistivity rises ~0.4%/°C. A DCR quoted at 20 °C understates the loss
of a winding running at 100 °C by ~31%. Every copper-loss number should state
its assumed winding temperature; this project computes at 20 °C and documents
that as a known simplification.

## AC copper loss (the part DC resistance misses)

At switching frequencies, current doesn't use the full wire cross-section:

- **Skin effect** — current crowds toward the surface
- **Proximity effect** — neighboring turns' fields push current into even
  smaller regions; in multi-layer windings this dominates and grows steeply
  with layer count

The combined effect is expressed as an AC resistance factor:

```
R_ac = F_r × R_dc,   F_r ≥ 1
```

At 50 kHz with thin wire, `F_r` may be ~1.05; at 500 kHz with thick wire in
3 layers it can exceed 5–10. Ignoring it flatters high-frequency designs.
Details and the skin-depth formula: [[skin-and-proximity-effect]].

## Copper vs core loss: the design balance

- More turns → more copper loss (longer wire), but less core loss
  (lower flux swing per turn).
- Fewer turns → the reverse.
- A loss-optimal design roughly balances the two. This is why ranking
  candidate designs by **total** loss (copper + core) picks different winners
  than ranking by size — the smallest core is often not the lowest-loss one.
