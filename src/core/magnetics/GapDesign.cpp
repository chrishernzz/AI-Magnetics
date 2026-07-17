#include "core/magnetics/GapDesign.h"

namespace {
constexpr double kPi = 3.14159265358979323846;
}

//precondition: aeCm2 > 0, muR > 0
//postcondition: returns the gapped-core AL (nH/turn^2). gapCm=0 reproduces the ungapped catalog value.
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
//postcondition: returns how large of an air gap it needs to get a desired inductance with a fixed nubmber of turns
double calculateRequiredGapCm(int turns, double aeCm2, double leCm, double muR, double targetInductanceNh) {
    //base case checking inducatance or turns are not less than 0 or equal
    if (targetInductanceNh <= 0.0 || turns <= 0) {
        return 0.0;
    }
    double n = static_cast<double>(turns);
    /*
    Original formula is: L(nH) = 0.4*pi * N^2 * Ae_cm2 * 10 / (Le_cm/muR + gapCm)
     => gapCm = 0.4*pi * N^2 * Ae_cm2 * 10 / L_nH - Le_cm/muR
    */
    double totalEffectiveLengthCm = 0.4 * kPi * n * n * aeCm2 * 10.0 / targetInductanceNh;
    //subtract the core contribution to get only the required air gaprit
    return totalEffectiveLengthCm - (leCm / muR);
}
