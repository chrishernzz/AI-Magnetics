#include "core/losses/CoreLoss.h"
#include <cmath>

// precondition: cuLossFactor is a real, non-placeholder coefficient
// postcondition: returns an approximate core loss density (W/cm^3) - see
// header for why this is a simplified model, not a validated Steinmetz fit
double calculateCoreLoss(double cuLossFactor, double fluxDensitySwingT, double switchingFreqHz) {
    return cuLossFactor * (switchingFreqHz / 100000.0) * std::pow(fluxDensitySwingT / 0.1, 2.0);
}
