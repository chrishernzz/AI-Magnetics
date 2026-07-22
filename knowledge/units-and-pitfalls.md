# Units and Classic Pitfalls

Magnetics design mixes SI, CGS-flavored engineering units, and datasheet
conventions. Most "mysterious" wrong answers are unit errors. State units on
every number.

## The conversions that actually come up

| Quantity | Common units | Conversion |
|---|---|---|
| Inductance | µH ↔ nH ↔ H | 1 µH = 1000 nH = 10⁻⁶ H |
| Core area Ae | mm² ↔ cm² ↔ m² | 100 mm² = 1 cm² = 10⁻⁴ m² |
| Area product | cm⁴ | Ae(cm²) × Wa(cm²) |
| Path length | mm ↔ cm | 10 mm = 1 cm |
| Volume | mm³ ↔ cm³ ↔ m³ | 10³ mm³ = 1 cm³ = 10⁻⁶ m³ |
| Flux density | T ↔ mT ↔ gauss | 1 T = 1000 mT = 10⁴ G |
| Current density | A/cm² ↔ A/mm² | 400 A/cm² = 4 A/mm² |
| Loss density | W/m³ ↔ W/cm³ ↔ mW/cm³ | 1 W/cm³ = 10⁶ W/m³; 1 mW/cm³ = 10³ W/m³ |

## Pitfalls this project has actually hit or guards against

1. **Steinmetz coefficients in the wrong volume convention.** SI-fitted
   coefficients (W/m³, e.g. PyOpenMagnetics `volumetricLosses`) used as if
   W/cm³ → results off by 10⁶. Real incident: a 0.06 W core loss computed as
   60,000 W. Check the source convention, then check a result for physical
   plausibility (a small E-core cannot dissipate kilowatts).
2. **kHz vs Hz in exponentials.** `f^1.4` at 80,000 vs 80 differs by ~10⁴.
   Steinmetz fits specify their frequency unit; these coefficients use Hz.
3. **Peak vs RMS confusion.** Flux/saturation ← peak; heating/wire ← RMS.
   Deriving one from the other without the waveform is fabrication
   (see [[peak-vs-rms-vs-ripple-current]]).
4. **AL in nH/T² vs µH/100T².** Powder-core datasheets often quote
   µH per 100 turns²; `L = AL·N²` silently breaks if conventions mix.
   nH/T² = (µH/100T²) × 10.
5. **20 °C copper resistance used for a hot winding** — understates DCR ~31%
   at 100 °C.
6. **Bsat at 25 °C used for a hot core** — overstates headroom by 20–30%.

## The meta-rule

A number without a unit, a temperature, and (for fitted coefficients) a
validity range is not data — it's a rumor. Design tools should refuse to
compute with rumors and say so, rather than produce confident nonsense.
