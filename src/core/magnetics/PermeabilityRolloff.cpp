#include "core/magnetics/PermeabilityRolloff.h"
#include "core/units/UnitConversions.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
}
//precondition: none
//postcondition: returns the information from the DCBiasCurveDatabase
DCBiasCurveLookup findDCBiasCurve(const std::string& materialName) {
    DCBiasCurveLookup result;

    //look in the Db and find the row that matches the material name and get the curve data for that material
    for (const auto& row : DCBiasCurveDatabase::load()) {
        if (row.materialName == materialName) {
            result.found = true;
            result.curve = row;
            return result;
        }
    }

    return result;
}

//precondition: turns > 0, leMm > 0
//postcondition: returns the DC magnetizing force H (Oersteds) - H = 0.4*pi*N*I/le, le in cm - the standard formula used by both Magnetics Inc. and Micrometals' own DC-bias curves (confirmed against their published
//worked examples). currentA may be 0 (returns H=0, the curve's own 100%-permeability point).
double dcMagnetizingForceOe(int turns, double currentA, double leMm) {
    double leCm = units::mmToCm(leMm);
    if (leCm <= 0.0) {
        return 0.0;
    }
    return 0.4 * kPi * static_cast<double>(turns) * currentA / leCm;
}

//precondition: curve came from a real DCBiasCurveDatabase row (DCBiasCurveLookup::found == true), hOe >= 0
//postcondition: returns %initial permeability remaining (0-100 scale, not a 0-1 fraction) via %mu = 1/(a + b*H^c) + d - clamped to [0, 100] since the raw formula can drift fractionally above 100 at
//H=0 for some materials' published coefficients (floating-point rounding on real datasheet values, not a sign the physics is wrong) and must never be read as more than fully-initial permeability.
double percentInitialPermeability(double hOe, const DCBiasCurveData& curve) {
    //the denominator is creating the shape of the permability-vs-H curve. As H increases, the denominator increases, causing the remaining percentage of initial permeability to decrease in the same way of the manufacture measured DC-bias curve decrease.
    double denominator = curve.a + curve.b * std::pow(hOe, curve.c);
    if (denominator <= 0.0) {
        return 0.0;
    }
    double percent = (1.0 / denominator) + curve.d;
    return std::clamp(percent, 0.0, 100.0);
}
