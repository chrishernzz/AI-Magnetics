#pragma once

/*
STAGE 4: Air gap design
Series-reluctance gapped-core model (McLyman-style CGS AL formula) -
originally verified numerically against a real ferrite catalog row
(E100/60/28-3C90: Ae=735.05 mm^2, Le=273.92 mm, mu=2249.28), where
calculateEffectiveAlNhPerTurnSq with gapCm=0 reproduced the catalog AL
(7584.86 nH/turn^2) to <0.03%. That row no longer exists in
data/real_cores.csv - the database was deliberately narrowed to Magnetics
MPP powder toroids and powder E-cores only, which have no discrete gap
(see PermeabilityRolloff.h) and never exercise this formula. This formula
therefore no longer has a live catalog row to check itself against; the
derivation and the <0.03% figure above are a historical accuracy record,
not a currently-checked invariant. tests/cpp/GapDesignTests.cpp exercises
the same numbers as an illustrative/synthetic fixture, not a live lookup.

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
