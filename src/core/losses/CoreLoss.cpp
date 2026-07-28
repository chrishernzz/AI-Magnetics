#include "core/losses/CoreLoss.h"
#include <cmath>

//precondition: cuLossFactor is a real, non-placeholder coefficient
//postcondition: returns an approximate core loss density (W/cm^3) - see header for why this is a simplified model, not a validated Steinmetz fit
double calculateCoreLoss(double cuLossFactor, double fluxDensitySwingT, double switchingFreqHz) {
    return cuLossFactor * (switchingFreqHz / 100000.0) * std::pow(fluxDensitySwingT / 0.1, 2.0);
}

//precondition: none
//postcondition: returns the CoreLossCoefficientDatabase row for materialName whose [minFreqHz, maxFreqHz) range contains switchingFreqHz, if any
//found == false if the material has no row at all, or has rows but none covering this frequency. 
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

//precondition: coefficients came from a real CoreLossCoefficientDatabase row (CoreLossCoefficientLookup::found == true)
//postcondition: returns core loss density (W/m^3) via Pv = k * f^alpha * B^beta - see header for the units and temperature-correction caveats
double calculateCoreLossDensity(const CoreLossCoefficientData& coefficients, double fluxDensitySwingT, double switchingFreqHz) {
    return coefficients.k * std::pow(switchingFreqHz, coefficients.alpha) * std::pow(fluxDensitySwingT, coefficients.beta);
}

//precondition: none
//postcondition: see header
bool fluxSwingWithinValidatedRange(const CoreLossCoefficientData& coefficients, double fluxDensitySwingT) {
    if (coefficients.minFluxSwingT.has_value() && fluxDensitySwingT < *coefficients.minFluxSwingT) {
        return false;
    }
    if (coefficients.maxFluxSwingT.has_value() && fluxDensitySwingT > *coefficients.maxFluxSwingT) {
        return false;
    }
    return true;
}
