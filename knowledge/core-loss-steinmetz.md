# Core Loss and the Steinmetz Equation

## What core loss is

Energy dissipated *in the core material itself* each switching cycle, from
magnetic hysteresis and eddy currents. It depends on how hard and how fast the
flux swings — not on the DC flux level.

## The Steinmetz equation

The standard empirical model:

```
Pv = k · f^α · B^β
```

- `Pv` — loss **density**. In the SI convention used by manufacturer-fitted
  coefficients (and this project's data): **W/m³**
- `f` — frequency in **Hz**
- `B` — **half** the peak-to-peak flux swing, or the swing itself — depends on
  the fitting convention; use coefficients consistently with how they were fit
- `k, α, β` — fitted per material, valid only over the frequency range they
  were fit for. Typical α ≈ 1.2–1.8, β ≈ 2.3–3.0

Total core loss:

```
P_core = Pv × Ve        (Ve = effective core volume, m³)
```

## ⚠️ The units trap (this project hit it)

Steinmetz coefficients circulate in at least three unit conventions:
**W/m³** (SI), **mW/cm³** (datasheet graphs), **W/cm³**. Same formula, factors
of 10³–10⁶ apart. This project once computed 60,000 W of "core loss" for a
0.06 W candidate by assuming W/cm³ for coefficients that were actually SI
W/m³. Always confirm the convention of the coefficient source before using it.
The PyOpenMagnetics/MAS field is literally named `volumetricLosses` — SI, W/m³.

## Flux swing comes from ripple current

```
ΔB = L · ΔI / (N · Ae)
```

- `ΔI` — the real peak-to-peak ripple current
- Never approximate ΔB from peak flux: peak flux includes the DC bias, which
  causes no AC loss. Using it can overestimate core loss by orders of
  magnitude. If ripple current is unknown, the honest answer is "core loss
  cannot be evaluated," not a guess.

## Temperature dependence

Ferrite core loss varies with temperature, usually with a minimum near
60–100 °C (that's what materials are optimized for). Manufacturer data
provides correction polynomials (ct0/ct1/ct2 coefficients). A calculation
without temperature correction is implicitly at the reference temperature of
the fit — acceptable as a documented simplification, wrong as a silent one.

## What makes core loss large or small

- **Bigger ripple current** → bigger ΔB → much bigger loss (β ≈ 2.5–3 power!)
- **More turns** → smaller ΔB per amp of ripple → less core loss (but more
  copper loss — see [[copper-loss]] for the balance)
- **Higher frequency** → more loss per the α exponent
- **Bigger core volume** → proportionally more total watts at the same Pv
