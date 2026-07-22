# Air Gaps and Energy Storage

## Why power inductors need a gap

Energy density in a magnetic field is `B²/(2µ)` — inversely proportional to
permeability. High-permeability ferrite (µr ≈ 2000) therefore stores almost
**no** energy itself: it saturates long before meaningful energy accumulates.

Cutting a small air gap into the magnetic path fixes this. The gap's µr = 1
means nearly all the stored energy lives *in the gap*, while the ferrite
merely guides the flux. A gapped ferrite core stores orders of magnitude more
energy than the same core ungapped, at the cost of reduced inductance per
turn.

Powder cores achieve the same thing with a *distributed* gap — insulating
binder between metal particles — which is why they need no machined gap
(see [[core-materials]]).

## The gapped-core math (series reluctance model)

The gap and core act as magnetic resistances (reluctances) in series. With
lengths in cm, areas in cm², inductances in nH:

```
AL0 = 0.4π · µr · Ae / Le × 10          (ungapped AL, nH/turn²)
lg  = 0.4π · N² · Ae × 10 / L_target − Le/µr     (required gap, cm)
AL_eff = 0.4π · Ae × 10 / (Le/µr + lg)  (gapped AL, nH/turn²)
```

- `Ae` — effective core area (cm²), `Le` — magnetic path length (cm)
- `AL` — inductance per turn² (`L = AL · N²`)
- The gap *reduces* AL, so a gapped design needs more turns for the same L —
  the trade for not saturating.

This project verified these formulas against real Ferroxcube core data to
better than 0.03%.

## Practical gap constraints

- **Manufacturing step**: gaps are ground in discrete steps; this project
  rounds to 0.01 mm.
- **Maximum useful gap**: past roughly 30–40% of... practical designs, more
  gap stops buying energy storage because flux increasingly *fringes* around
  the gap. This project rejects designs needing a gap over 40% of the path
  length.
- **Fringing flux**: the field bulging around the gap effectively enlarges the
  gap area, making real inductance a few percent *higher* than the simple
  formula predicts. It also induces eddy losses in winding turns placed near
  the gap — keep the first winding layer away from the gap region. The simple
  series-reluctance model here ignores fringing; that is a known accuracy
  limitation, typically a few percent for small gaps.
