#include "core/losses/CoreLoss.h"
#include <cmath>

// precondition: cuLossFactor is a real, non-placeholder coefficient
// postcondition: returns an approximate core loss density (W/cm^3) - see
// header for why this is a simplified model, not a validated Steinmetz fit
double calculateCoreLoss(double cuLossFactor, double fluxDensitySwingT, double switchingFreqHz) {
    return cuLossFactor * (switchingFreqHz / 100000.0) * std::pow(fluxDensitySwingT / 0.1, 2.0);
}

//precondition: none
//postcondition: see header
CoreLossCoefficientLookup findCoreLossCoefficients(const std::string& materialName, double switchingFreqHz) {
    CoreLossCoefficientLookup result;

    for (const auto& row : CoreLossCoefficientDatabase::load()) {
        if (row.materialName != materialName) {
            continue;
        }
        if (switchingFreqHz >= row.minFreqHz && switchingFreqHz < row.maxFreqHz) {
            result.found = true;
            result.coefficients = row;
            return result;
        }
    }

    return result;
}
