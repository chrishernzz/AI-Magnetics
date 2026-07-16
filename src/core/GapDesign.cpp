#include "GapDesign.h"

namespace {
constexpr double kPi = 3.14159265358979323846;
}

// precondition: aeCm2 > 0, muR > 0
// postcondition: returns the gapped-core AL (nH/turn^2). gapCm=0 reproduces
// the ungapped catalog value.
double calculateEffectiveAlNhPerTurnSq(double aeCm2, double leCm, double muR, double gapCm) {
    double effectiveLengthCm = (leCm / muR) + gapCm;
    if (effectiveLengthCm <= 0.0) {
        return 0.0;
    }
    // AL(nH/turn^2) = 0.4*pi * Ae_cm2 * 10 / (Le_cm/muR + gapCm)
    return 0.4 * kPi * aeCm2 * 10.0 / effectiveLengthCm;
}

// precondition: turns > 0, targetInductanceNh > 0, aeCm2 > 0, muR > 0
// postcondition: returns the gap length (cm) needed for `turns` turns to
// realize targetInductanceNh on this core.
double calculateRequiredGapCm(int turns, double aeCm2, double leCm, double muR, double targetInductanceNh) {
    if (targetInductanceNh <= 0.0 || turns <= 0) {
        return 0.0;
    }
    double n = static_cast<double>(turns);
    // L(nH) = 0.4*pi * N^2 * Ae_cm2 * 10 / (Le_cm/muR + gapCm)
    // => gapCm = 0.4*pi * N^2 * Ae_cm2 * 10 / L_nH - Le_cm/muR
    double totalEffectiveLengthCm = 0.4 * kPi * n * n * aeCm2 * 10.0 / targetInductanceNh;
    return totalEffectiveLengthCm - (leCm / muR);
}
