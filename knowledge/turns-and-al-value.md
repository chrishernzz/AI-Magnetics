# Turns Count and the AL Value

## AL: inductance per turn squared

Core datasheets give `AL`, usually in nH/turn². It compresses the core's
geometry and permeability into one number:

```
L = AL · N²        (L in nH when AL is nH/T²)
```

So the first-pass turns count for a target inductance is:

```
N = round( sqrt( L_target / AL ) )
```

with both in consistent units (e.g. L in nH, AL in nH/T²).

## Why turns and gap must be designed *together*

For a gapped design the loop is circular: the gap changes AL, and the turns
needed depend on AL. The practical algorithm iterates:

1. Seed N from the **ungapped** AL (formula above)
2. Compute the gap required for target L at this N
3. Compute the **gapped** (effective) AL with that gap
4. Recompute N from the gapped AL
5. Repeat until the integer N stops changing (typically 2–4 iterations)

The result is an (N, gap) pair whose computed inductance should land within
the design tolerance (±10% default in this project) of the target.

## Integer turns: the quantization error

N must be a whole number (you can't wind 8.4 turns on an E-core bobbin*), so
the achieved inductance rarely equals the target exactly. With few turns the
steps are coarse: going from 8 to 9 turns changes L by ~27% (`9²/8² = 1.27`).
This is why low-turn-count designs often show several-percent inductance
error — it's quantization, not a bug. The gap provides the fine adjustment.

*Toroids can effectively have "partial" turns via winding placement, but
standard practice still counts whole passes through the window.

## Sanity checks

- More gap → lower AL → more turns. If a design shows *fewer* turns with a
  *bigger* gap at the same L, something is inconsistent.
- `AL` from the datasheet is a nominal value, often ±25% for ungapped ferrite
  (permeability tolerance). Gapped cores have much tighter effective-AL
  tolerance — another reason gapped designs are preferred for accurate L.
