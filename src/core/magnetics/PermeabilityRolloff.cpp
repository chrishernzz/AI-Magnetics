#include "core/magnetics/PermeabilityRolloff.h"
#include "core/units/UnitConversions.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
}

//precondition: none
//postcondition: see header
DCBiasCurveLookup findDCBiasCurve(const std::string& materialName) {
    DCBiasCurveLookup result;

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
//postcondition: see header. H = 0.4*pi*N*I/le, le in cm (confirmed against Magnetics Inc.'s own worked
//example: a 90-turn, 60mu Kool Mu core at a real magnetic path length reaches H=57.5 Oe at their stated
//current in that example).
double dcMagnetizingForceOe(int turns, double currentA, double leMm) {
    double leCm = units::mmToCm(leMm);
    if (leCm <= 0.0) {
        return 0.0;
    }
    return 0.4 * kPi * static_cast<double>(turns) * currentA / leCm;
}

//precondition: curve came from a real DCBiasCurveDatabase row, hOe >= 0
//postcondition: see header
double percentInitialPermeability(double hOe, const DCBiasCurveData& curve) {
    double denominator = curve.a + curve.b * std::pow(hOe, curve.c);
    if (denominator <= 0.0) {
        return 0.0;
    }
    double percent = (1.0 / denominator) + curve.d;
    return std::clamp(percent, 0.0, 100.0);
}
