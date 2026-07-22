# Thermal Basics for Inductors

## The core question

Total loss (copper + core) becomes heat. The inductor's temperature rise is:

```
ΔT = P_total × Rth        (°C)
```

- `Rth` — thermal resistance from the winding/core to ambient (°C/W). It
  depends on surface area, orientation, airflow, and mounting — it is a
  property of the *built part in its environment*, not of the core alone.

## Rough estimation methods

Without measured Rth, engineers use empirical correlations, e.g. the common
surface-area estimate for natural convection:

```
Rth ≈ 850 / As        (°C/W, As = exposed surface area in cm²)
```

(Various constants 700–1000 circulate; all are ±30% at best.) Datasheet Rth or
measurement beats any formula.

## Why temperature limits matter twice

1. **Insulation and materials** — magnet wire insulation classes (130 °C,
   155 °C, 180 °C), bobbin plastics, and potting all have hard limits.
2. **The magnetics themselves shift** — Bsat drops as the core heats
   (saturation gets closer at exactly the moment loss is heating the part),
   copper resistance rises ~0.4%/°C (more loss), and ferrite core loss vs
   temperature has a minimum near 60–100 °C. A design evaluated only at 25 °C
   is optimistic on nearly every axis.

## The feedback loop

Loss → temperature rise → higher copper resistance and shifted core loss →
different loss. Strictly this converges by iteration; in practice one round of
"compute loss at estimated hot temperature" is usually adequate at power
levels where an area-product design flow applies.

## Status in this project

Thermal rise is reported `not_evaluated` — no thermal-resistance data exists
in the core library yet, and the tool refuses to fake it. The allowable
temperature rise input is accepted and threaded through so a real model can
plug in later. First implementable step: the surface-area correlation above,
computed from core dimensions, clearly labeled as an estimate.
