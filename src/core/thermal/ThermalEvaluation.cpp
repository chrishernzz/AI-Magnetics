#include "core/thermal/ThermalEvaluation.h"
#include <cmath>

namespace {

//precondition: none
//postcondition: returns rules.defaultThermalResistanceCPerW (and sets isGeometryDerivedOut=false) when
//aeMm2 or leMm is not positive - geometry unknown, never divide by zero or fabricate a number. Otherwise
//returns a Newton's-law-of-cooling estimate, Rth = 1/(h*surfaceAreaM2), where surfaceAreaM2 approximates
//the core's total external surface area as a compact solid of the same real magnetic volume (Ae*Le) - see
//DesignRules.h for the sourcing of h and the shape-factor simplification.
double estimateThermalResistanceCPerW(double aeMm2, double leMm, const DesignRules& rules, bool& isGeometryDerivedOut) {
    isGeometryDerivedOut = false;
    if (aeMm2 <= 0.0 || leMm <= 0.0) {
        return rules.defaultThermalResistanceCPerW;
    }
    constexpr double kMm3ToM3 = 1e-9;
    double volumeM3 = aeMm2 * leMm * kMm3ToM3;
    double surfaceAreaM2 = rules.compactSolidSurfaceAreaShapeFactor * std::pow(volumeM3, 2.0 / 3.0);
    if (surfaceAreaM2 <= 0.0 || rules.naturalConvectionCoefficientWPerM2K <= 0.0) {
        return rules.defaultThermalResistanceCPerW;
    }
    isGeometryDerivedOut = true;
    return 1.0 / (rules.naturalConvectionCoefficientWPerM2K * surfaceAreaM2);
}

}  //namespace

//precondition: none
//postcondition: see block comment above. Iterates up to rules.maxThermalIterations times (mirrors TurnsAndGapDesign.cpp's kMaxIterations pattern), converged when the winding-temperature change between
//iterations is below rules.thermalConvergenceThresholdC. Returns NotEvaluated (not a stale intermediate number) when copperLossGeometryKnown is false, or when the loop fails to converge within the iteration cap.
ThermalEvaluationResult evaluateThermal(const ThermalIterationInputs& inputs, const DesignRules& rules) {
    ThermalEvaluationResult result;

    if (!inputs.copperLossGeometryKnown) {
        result.status = ThermalStatus::NotEvaluated;
        result.missingDataExplanation = "cannot seed a hot-DCR estimate without known winding DCR geometry (see WindingDesignResult::resistanceStatus)";
        return result;
    }

    bool thermalResistanceIsGeometryDerived = false;
    double thermalResistanceCPerWUsed = estimateThermalResistanceCPerW(inputs.coreEffectiveAreaMm2, inputs.coreMagneticPathLengthMm, rules, thermalResistanceIsGeometryDerived);

    double windingTempC = inputs.ambientTemperatureC;
    double hotDcrOhms = inputs.coldDcrOhmsAt20C;
    double copperLossW = 0.0;
    double knownLossW = 0.0;
    double riseC = 0.0;
    bool converged = false;
    int iteration = 0;

    for (; iteration < rules.maxThermalIterations; ++iteration) {
        //Hot DCR at the current winding-temperature estimate, then the loss and rise it implies.
        hotDcrOhms = inputs.coldDcrOhmsAt20C * (1.0 + rules.copperTempCoefficientPerC * (windingTempC - 20.0));
        copperLossW = inputs.rmsCurrentA * inputs.rmsCurrentA * hotDcrOhms;
        knownLossW = copperLossW + (inputs.coreLossKnown ? inputs.coreLossW : 0.0);
        riseC = knownLossW * thermalResistanceCPerWUsed;
        double newWindingTempC = inputs.ambientTemperatureC + riseC;

        bool stabilized = std::abs(newWindingTempC - windingTempC) < rules.thermalConvergenceThresholdC;
        windingTempC = newWindingTempC;

        if (stabilized) {
            converged = true;
            ++iteration;
            break;
        }
    }

    if (!converged) {
        //Genuine positive-feedback divergence (see header) for a high-current/low-DCR design is a real, reachable case here, not just a numerical edge case - the last computed intermediate temperature
        //is not a meaningful estimate in that case, so it is not reported as one.
        result.status = ThermalStatus::NotEvaluated;
        result.missingDataExplanation = "thermal loop did not converge within " + std::to_string(rules.maxThermalIterations) + " iterations - this heuristic's positive feedback (hotter winding raises DCR, which raises loss, which raises temperature further) did not stabilize, which can indicate a thermal-runaway-prone operating point rather than a benign numerical issue; no estimate is reported";
        return result;
    }

    result.status = ThermalStatus::PreliminaryThermalEstimate;
    result.convergedWindingTempC = windingTempC;
    result.hotDcrOhms = hotDcrOhms;
    result.copperLossAtConvergedTempW = copperLossW;
    result.knownLossW = knownLossW;
    result.predictedTempRiseC = riseC;
    result.predictedHotspotTempC = windingTempC;
    result.iterationsUsed = iteration;
    result.converged = true;
    result.thermalResistanceCPerWUsed = thermalResistanceCPerWUsed;
    result.thermalResistanceIsGeometryDerived = thermalResistanceIsGeometryDerived;
    return result;
}
