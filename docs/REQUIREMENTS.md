# Requirements traceability

Maps each input/output your boss specified to the file that owns it.

| Requirement (from boss) | Owned by |
|---|---|
| L (given) | `AreaProduct.cpp` (consumes it), `TurnsCalculation.cpp` (consumes it) |
| Peak current | `AreaProduct.cpp`, `CopperLoss.cpp` |
| Current waveform | `AreaProduct.cpp`, `HighFrequencyLosses.cpp` |
| Switching frequency | `MaterialSelection.cpp`, `AreaProduct.cpp`, `CoreLoss.cpp` |
| Allowable temp rise | `MaterialSelection.cpp`, `CopperLoss.cpp` |
| Ap / core selection (McLyman p.63) | `AreaProduct.cpp`, `CoreSelection.cpp`, `data/cores/` |
| Gapped core branch | `GapDesign.cpp` |
| Turns | `TurnsCalculation.cpp` |
| Copper / core / high-freq losses | `CopperLoss.cpp`, `CoreLoss.cpp`, `HighFrequencyLosses.cpp` |
