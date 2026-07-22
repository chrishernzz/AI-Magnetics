# Area Product (Ap) Core Sizing

The area-product method (McLyman, *Transformer and Inductor Design Handbook*,
p. 63) answers the first sizing question: **how big must the core be?**

## The concept

A core's capability is captured by the product of two areas:

```
Ap = Ae × Wa      (cm⁴)
```

- `Ae` — effective magnetic cross-section (how much flux it can carry)
- `Wa` — winding window area (how much copper fits)

An inductor needs both: flux capacity for the energy, window for the turns.
One number, `Ap`, encodes the pair.

## The sizing formula

```
Ap_required = (2 · E · 10⁴) / (Ku · Bmax · J)      (cm⁴)
```

- `E = ½·L·Ipk²` — stored energy (joules) — uses **peak** current
- `Ku` — window utilization (fraction of window actually filled with copper);
  0.4 is the classic default for round wire
- `Bmax` — allowed peak flux density (T)
- `J` — allowed current density (A/cm²); 400 A/cm² is a common convective-
  cooling default

Any core with `Ap ≥ Ap_required` is *big enough to try*; the detailed
turns/gap/winding/loss design then confirms or rejects it.

## What it is and isn't

- It **is** a fast, physics-grounded filter that eliminates hopeless cores
  before detailed design.
- It is **not** a guarantee — a core passing Ap can still fail winding fit,
  saturation with the actual turn count, or thermal limits.
- If **no** core in the library meets Ap_required, the honest answer is
  "no feasible design in this library" — not silently picking the biggest
  available core and hoping. (This project once had that silent fallback;
  it was removed deliberately.)

## Questions worth asking

1. **"Is the peak current the true worst case?"** — Ap scales with Ipk², so an
   underestimated peak undersizes the core badly.
2. **"Natural convection or forced air?"** — J = 400 A/cm² assumes still air;
   forced air allows more, sealed enclosures less.
3. **"Round wire or foil/litz?"** — Ku = 0.4 is a round-wire number; foil can
   reach 0.6+, litz less than 0.4.
