# What Is an Inductor

An inductor is a passive two-terminal component that stores energy in a
magnetic field when current flows through it. Physically it is a coil of wire
(the winding) usually wound around a magnetic core (ferrite or powdered metal).

## The defining behavior

An inductor resists *changes* in current:

```
V = L · di/dt
```

- `V` — voltage across the inductor (volts)
- `L` — inductance (henries, H; power inductors are usually µH)
- `di/dt` — how fast the current is changing (amps per second)

A steady DC current sees only the winding's resistance; a fast-changing
current sees a large opposing voltage. This is the opposite of a capacitor,
which resists changes in *voltage*.

## Energy storage

The energy held in the magnetic field at current `I`:

```
E = ½ · L · I²        (joules, with L in H and I in A)
```

This formula is the starting point for physically sizing a power inductor —
the core must be big enough to hold this energy without saturating. In this
project it feeds the area-product calculation (see [[area-product-sizing]]).

## Why power converters need them

In a switching converter (buck, boost, flyback output, etc.) the inductor:
- **Smooths current** — converts the chopped switch-node voltage into a
  triangular ripple current instead of current spikes.
- **Transfers energy** — stores energy while the switch is on, releases it to
  the output while the switch is off.
- **Sets ripple** — for a buck converter the peak-to-peak ripple current is
  approximately `ΔI = Vout·(1-D)/(L·fsw)`; a bigger L means less ripple.

## The three quantities that get confused

| Quantity | Symbol | What it drives |
|---|---|---|
| Peak current | Ipk | Core size, peak flux density, saturation margin |
| RMS current | Irms | Wire size, copper loss, heating |
| Ripple current (p-p) | ΔI | Core loss (flux-density swing), output ripple |

These are three different numbers and must never be derived from one another
by guessing — see [[peak-vs-rms-vs-ripple-current]].
