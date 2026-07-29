#include "core/thermal/ThermalEvaluation.h"
#include <cmath>

//precondition: none
//postcondition: see header
ThermalEvaluationResult evaluateThermal(const ThermalIterationInputs& inputs, const DesignRules& rules) {
    ThermalEvaluationResult result;

    if (!inputs.copperLossGeometryKnown) {
        result.status = ThermalStatus::NotEvaluated;
        result.missingDataExplanation = "cannot seed a hot-DCR estimate without known winding DCR geometry (see WindingDesignResult::resistanceStatus)";
        return result;
    }

    double windingTempC = inputs.ambientTemperatureC;
    double hotDcrOhms = inputs.coldDcrOhmsAt20C;
    double copperLossW = 0.0;
    double knownLossW = 0.0;
    double riseC = 0.0;
    bool converged = false;
    int iteration = 0;

    for (; iteration < rules.maxThermalIterations; ++iteration) {
        // Hot DCR at the current winding-temperature estimate, then the loss and rise it implies.
        hotDcrOhms = inputs.coldDcrOhmsAt20C * (1.0 + rules.copperTempCoefficientPerC * (windingTempC - 20.0));
        copperLossW = inputs.rmsCurrentA * inputs.rmsCurrentA * hotDcrOhms;
        knownLossW = copperLossW + (inputs.coreLossKnown ? inputs.coreLossW : 0.0);
        riseC = knownLossW * rules.defaultThermalResistanceCPerW;
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
        // Genuine positive-feedback divergence (see header) for a high-current/low-DCR design is a real,
        // reachable case here, not just a numerical edge case - the last computed intermediate temperature
        // is not a meaningful estimate in that case, so it is not reported as one.
        result.status = ThermalStatus::NotEvaluated;
        result.missingDataExplanation =
            "thermal loop did not converge within " + std::to_string(rules.maxThermalIterations) +
            " iterations - this heuristic's positive feedback (hotter winding raises DCR, which raises loss, "
            "which raises temperature further) did not stabilize, which can indicate a thermal-runaway-prone "
            "operating point rather than a benign numerical issue; no estimate is reported";
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
    result.thermalResistanceCPerWUsed = rules.defaultThermalResistanceCPerW;
    return result;
}
