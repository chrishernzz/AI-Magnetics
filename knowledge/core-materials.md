# Core Materials for Power Inductors

Two big families dominate power inductor design: **ferrites** and **powder
cores**. The choice shapes everything downstream — gapping, saturation
behavior, loss profile, cost.

## MnZn power ferrites

Examples in this project's data: Ferroxcube 3C90/3C92/3C94/3C95/3C96/3C97
(and higher-frequency 3F36/3F46), TDK N27/N41/N87/N49, Fair-Rite 95/98.

- **High permeability** (µr ≈ 1500–3000) — very little energy storage in the
  material itself, so power inductors made from ferrite **require a discrete
  air gap** (see [[air-gaps-and-energy-storage]]).
- **Low core loss** at their rated frequencies — the loss champion for
  50 kHz–1 MHz designs.
- **Hard saturation** around 0.35–0.5 T, worse when hot.
- Made as shaped cores (E, ETD, RM, pot, toroid) that can be ground for a gap.

## Powder cores (distributed gap)

Examples in this project's data: Magnetics Kool Mµ / Kool Mµ MAX / Kool Mµ Hƒ,
XFlux, Micrometals/Fair-Rite iron powder grades (material codes like 26, 40,
60, and Fair-Rite "78"/"79"/"80" toroids).

- **Low effective permeability by design** (µr ≈ 26–125) — the material is
  metal powder in an insulating binder, so the "air gap" is distributed
  throughout the material. **No machined gap needed or possible.**
- **Soft saturation** — inductance falls gradually with DC bias instead of
  cliffing. Many tolerate much higher flux than ferrite (XFlux usefully
  above 1 T).
- **Higher core loss than ferrite** at the same frequency/flux — the usual
  price for the distributed gap and high Bsat.
- Almost always toroids or blocks; commonly used for high-DC-bias chokes.

## How to choose (first-order)

| Requirement | Leans toward |
|---|---|
| Lowest possible core loss at 100 kHz–1 MHz | Gapped ferrite |
| High DC bias / soft overload behavior | Powder core |
| Tight inductance vs load (flat L) | Gapped ferrite |
| No gap fringing field allowed (EMI-sensitive) | Powder toroid |
| Lowest cost at moderate performance | Iron powder |

## Modeling caveat for this project

The current engine applies one gapped-core physics model to every candidate.
For powder toroids that returns gap ≈ 0.00 mm (numerically harmless, since
their permeability already encodes the distributed gap), but their soft
saturation vs DC bias is **not** modeled — the single Bsat check understates
how much inductance a powder core loses at high current. A proper powder-core
branch would use the manufacturer's permeability-vs-H curves.
