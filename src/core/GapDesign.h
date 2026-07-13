// #pragma once

// #include "CoreSelection.h"

// // STAGE 4: Gapped Core? (branch)
// //
// // NEW — did not exist in the flyback version. A flyback transformer's
// // air gap logic was folded into TurnsAndInductance before; the buck
// // inductor workflow calls it out as its own explicit decision branch,
// // since most buck inductors need a gap to store energy without
// // saturating.
// //
// // If gapped: compute gap length and the resulting effective
// // permeability. If ungapped: pass the core's native permeability
// // through unchanged.

// struct GapResult {
//     bool isGapped;
//     double gapLengthCm;
//     double effectivePermeability;
// };

// GapResult designGap(const CoreParams& core, double inductanceH, double turnsEstimate);
