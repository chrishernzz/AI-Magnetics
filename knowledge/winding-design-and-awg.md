# Winding Design and Wire Gauge (AWG)

## Sizing the conductor

The wire must carry the **RMS** current without overheating. The design rule
is a maximum current density `J`:

```
A_required = Irms / J
```

With `J` in A/mm² (or A/cm² — watch units), `A_required` is the minimum
copper cross-section. Pick the smallest standard wire that meets it. This
project's default is J = 400 A/cm² = 4 A/mm² for natural convection.

## AWG basics

American Wire Gauge is logarithmic: **3 gauge steps ≈ half/double the
cross-sectional area**; 10 steps = 10× area. Useful anchors:

| AWG | Diameter (mm) | Area (mm²) | ~Ampacity at 4 A/mm² |
|---|---|---|---|
| 14 | 1.63 | 2.08 | 8.3 A |
| 18 | 1.02 | 0.823 | 3.3 A |
| 22 | 0.644 | 0.326 | 1.3 A |
| 26 | 0.405 | 0.129 | 0.5 A |
| 30 | 0.255 | 0.0509 | 0.2 A |

## Parallel strands

When a single wire would be impractically thick (stiff to wind, poor window
usage), use multiple thinner strands in parallel. Practical rule: pick a
maximum single-strand thickness (this project uses AWG 18) and parallel as
many as needed to reach the required area. At high frequency this shades into
litz wire territory (see [[skin-and-proximity-effect]]).

## DC resistance and wire length

```
wire_length = N × MLT
R_dc = ρ · wire_length / A_copper
```

- `MLT` — mean length per turn, a core/bobbin geometry property
- `ρ` — copper resistivity, 1.724×10⁻⁸ Ω·m at 20 °C, rising ~0.4%/°C —
  a winding at 100 °C has ~31% more resistance than at 20 °C. State the
  temperature any DCR number assumes.

## Fill factor check

After choosing wire and turns:

```
fill = (N × A_wire_with_insulation) / Wa
```

Practical winding fits below ~0.6 for round wire on a bobbin (this project's
limit); the classic *planning* number Ku = 0.4 already accounts for
insulation, bobbin, and imperfect layering. A computed fill above the limit
means the wire/turns combination physically will not fit — reject, don't hope.
