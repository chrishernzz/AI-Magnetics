# Skin and Proximity Effect (AC Winding Loss)

At switching frequencies, AC current stops using the wire's full
cross-section. Two mechanisms, one result: effective resistance rises.

## Skin effect

AC current concentrates in a surface layer of thickness δ (skin depth):

```
δ = sqrt( ρ / (π · µ · f) )
```

- `ρ` — conductor resistivity (copper: 1.724×10⁻⁸ Ω·m at 20 °C)
- `µ` — permeability of the conductor (copper: µ0 = 4π×10⁻⁷ H/m)
- `f` — frequency (Hz)

Copper skin depth at useful frequencies:

| f | δ |
|---|---|
| 50 kHz | 0.30 mm |
| 100 kHz | 0.21 mm |
| 500 kHz | 0.093 mm |
| 1 MHz | 0.066 mm |

Rule of thumb: a solid round wire's *diameter* should not exceed ~2δ, or its
interior is wasted. At 100 kHz that means nothing thicker than ~AWG 26 works
efficiently as a single solid strand.

## Proximity effect

The magnetic field of neighboring turns/layers forces current into even
smaller regions of each conductor. In multi-layer windings this **dominates**
skin effect and grows roughly with the square of the layer count. A 4-layer
winding of 2δ-thick wire can have several times its DC resistance.

## The combined model

```
R_ac = F_r × R_dc
F_r = F_skin + F_proximity
```

`F_r` is computed from the conductor-thickness-to-skin-depth ratio and the
number of layers (Dowell's method is the standard analytical approach; the
OpenMagnetics wire-adviser documents a practical formulation). AC copper loss
is then `P_cu_ac = Irms_ac² × (F_r − 1) × R_dc` on top of the DC loss.

## Mitigations

- **Litz wire** — many individually-insulated strands, each thinner than δ,
  woven so each strand occupies every position; the standard fix above
  ~100 kHz at high current
- **Fewer layers** — proximity punishes layers hardest; a single-layer
  winding is dramatically better than three
- **Foil windings** — thin foil ≤ 2δ, one turn per layer, excellent at high
  current/low turns

## Status in this project

Not yet implemented — high-frequency loss is honestly reported
`not_evaluated` rather than pretending R_dc is the whole story. The formulas
above (from the OpenMagnetics MKF wire-adviser research) are the intended
implementation basis.
