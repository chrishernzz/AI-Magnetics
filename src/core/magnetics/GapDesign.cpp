#include "core/magnetics/GapDesign.h"

namespace {
constexpr double kPi = 3.14159265358979323846;
}

//precondition: aeCm2 > 0, muR > 0
//precondition: returns Effective AL (nH/turn^2) of a core with effective area aeCm2 (cm^2), magnetic path length leCm (cm), relative permeability muR, and a gap of
//gapCm (cm) inserted in the magnetic path. gapCm = 0 reproduces the ungapped catalog AL.
double calculateEffectiveAlNhPerTurnSq(double aeCm2, double leCm, double muR, double gapCm) {
    //calculating the effective magnetic path length. The gap increases magnetic reluctance and lowers AL
    double effectiveLengthCm = (leCm / muR) + gapCm;

    //base case that prevents division by zero  
    if (effectiveLengthCm <= 0.0) {
        return 0.0;
    }
    //Calculate AL using: AL(nH/turn^2) = 0.4*pi * Ae_cm2 * 10 / (Le_cm/muR + gapCm)
    return 0.4 * kPi * aeCm2 * 10.0 / effectiveLengthCm;
}
//precondition: number of wire turn (turns > 0), desired inductance in nanhohenries(targetInductanceNh > 0), aeCm2 > 0, muR > 0
//postcondition: returns Gap length (cm) required so that `turns` turns on this core produce /targetInductanceNh. Returns zero or a negative value if the core's
//ungapped AL already meets or exceeds the target at this turns count (no gap needed - not an error condition).
double calculateRequiredGapCm(int turns, double aeCm2, double leCm, double muR, double targetInductanceNh) {
    //base case checking inducatance or turns are not less than 0 or equal
    if (targetInductanceNh <= 0.0 || turns <= 0) {
        return 0.0;
    }
    double n = static_cast<double>(turns);
    //Original formula is: L(nH) = 0.4*pi * N^2 * Ae_cm2 * 10 / (Le_cm/muR + gapCm) => gapCm = 0.4*pi * N^2 * Ae_cm2 * 10 / L_nH - Le_cm/muR
    double totalEffectiveLengthCm = 0.4 * kPi * n * n * aeCm2 * 10.0 / targetInductanceNh;
    //subtract the core contribution to get only the required air gaprit
    return totalEffectiveLengthCm - (leCm / muR);
}
