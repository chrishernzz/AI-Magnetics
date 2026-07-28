#pragma once

/*
STAGE 4: Air gap design
Series-reluctance gapped-core model (McLyman-style CGS AL formula) -
verified numerically against data/real_cores.csv: for E100/60/28-3C90
(Ae=735.05 mm^2, Le=273.92 mm, mu=2249.28), calculateEffectiveAlNhPerTurnSq
with gapCm=0 reproduces the catalog AL (7584.86 nH/turn^2) to <0.03%.

No fringing-flux correction (Fc factor) is applied in Phase 1 - none of
the loaded core data carries the winding/bobbin geometry a fringing model
would need. This is a documented simplification, not a silent omission.
*/

//Effective AL (nH/turn^2) of a core with effective area aeCm2 (cm^2),
//magnetic path length leCm (cm), relative permeability muR, and a gap of
//gapCm (cm) inserted in the magnetic path. gapCm = 0 reproduces the ungapped catalog AL.
double calculateEffectiveAlNhPerTurnSq(double aeCm2, double leCm, double muR, double gapCm);

//Gap length (cm) required so that `turns` turns on this core produce
//targetInductanceNh. Returns zero or a negative value if the core's
//ungapped AL already meets or exceeds the target at this turns count (no
//gap needed - not an error condition).
double calculateRequiredGapCm(int turns, double aeCm2, double leCm, double muR, double targetInductanceNh);
