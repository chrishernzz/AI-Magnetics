# Requirements traceability

Maps each input/output specified by your boss to the file that owns it, and whether it's actually implemented yet.

| Requirement (from boss) | Owned by | Status |
|---|---|---|
| L (given) | `AreaProduct.cpp` (consumes it) · `TurnsCalculation.cpp` (would consume it) | ✅ used in Area Product; ❌ Turns not implemented |
| Peak current | `AreaProduct.cpp` · `CopperLoss.cpp` | ✅ used in Area Product; ❌ Copper Loss not implemented |
| Current waveform | Declared as `waveformFactor` on `MaterialSelectionInput`; intended for `HighFrequencyLosses.cpp` | ❌ Accepted but not read anywhere yet — has no effect on any current output |
| Switching frequency | `MaterialSelection.cpp` · `AreaProduct.cpp` (indirectly, via caller) · `CoreLoss.cpp` (would consume it) | ✅ used in Material Selection; ❌ Core Loss not implemented |
| Allowable temp rise | Accepted by `MaterialSelectionInput`/`AreaProductInput`; intended for `CopperLoss.cpp` and a real validation check | ❌ Currently accepted and echoed back only — nothing checks it against predicted losses yet |
| Ap / core selection (McLyman p.63) | `AreaProduct.cpp`, `CoreSelection.cpp`, `data/cores.csv` | ✅ Implemented |
| Gapped core branch | `GapDesign.cpp` | ❌ Stub |
| Turns | `TurnsCalculation.cpp` | ❌ Stub |
| Copper / core / high-freq losses | `CopperLoss.cpp`, `CoreLoss.cpp`, `HighFrequencyLosses.cpp` | ❌ All stubs |
| Fill factor check | Intended for `src/validation/Validation.h` | ❌ Header-only struct, not compiled (`CMakeLists.txt`'s `VALIDATION_SOURCES` is empty) |
| Flux density check | Intended for `src/validation/Validation.h` | ❌ Same — not implemented |