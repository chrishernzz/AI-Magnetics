# Switching Frequency

The converter's PWM frequency (`fsw`), typically 50 kHz–2 MHz in modern power
electronics. It affects nearly every part of the inductor design.

## What higher frequency does

**Good:**
- Smaller inductance needed for the same ripple (`L ∝ 1/fsw`)
- Smaller core, smaller overall converter
- Faster transient response

**Bad:**
- Core loss rises steeply — Steinmetz `Pv = k·f^α·B^β` with α typically
  1.2–1.8, so doubling frequency can more than double loss density
- Skin and proximity effects get worse (see [[skin-and-proximity-effect]]) —
  above ~100 kHz solid round wire stops using its full cross-section
- Switching losses in the semiconductors rise
- Not every material works: each core material has a usable frequency range

## Material frequency compatibility

Materials are characterized for specific frequency ranges. Examples from this
project's real data snapshot (sourced from manufacturer data via
PyOpenMagnetics/MAS):

- **MnZn power ferrites** (3C90, N27, N87 class): roughly 25 kHz–500 kHz
  depending on grade; higher-frequency grades (3F36, 3F46, N49) reach 1–2 MHz+
- **Powder cores** (Kool Mµ, XFlux, iron powder): usable across a wide range,
  but loss rises quickly with frequency; commonly applied below ~500 kHz
- Using a material outside its characterized range means its loss data simply
  doesn't apply — a design tool should reject or flag it, not extrapolate.

## Questions worth asking when someone states a frequency

1. **"Is this fixed by your controller, or negotiable?"** — if negotiable, the
   frequency/core-size/loss trade can be explored.
2. **"Is it exactly this frequency, or spread-spectrum/variable?"** — variable
   frequency means the worst-case corner must be designed for.
3. **"80 kHz with a 1 MHz-rated material?"** — works, but a lower-frequency
   material might be cheaper and lower-loss at 80 kHz. Compatibility is
   necessary, not sufficient.
