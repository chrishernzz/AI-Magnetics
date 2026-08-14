#include "core/thermal/ThermalEvaluation.h"
#include <cmath>

namespace {

//precondition: Rth will be used that tells us how much the temperature rises for a given amount of power dissipated (energy spread out or transformed into a less useful form, typically heat)
//postcondition: returns a Newton's-law-of-cooling estimate, Rth (thermal resistance) = 1/(h*surfaceAreaM2), from a REAL manufacturer-published wound-coil surface area when coreWoundSurfaceAreaMm2 > 0 (sets
//usesRealSurfaceAreaOut=true); otherwise approximates surface area from the core's magnetic volume (Ae*Le) as a compact solid (sets isGeometryDerivedOut=true instead); otherwise returns
//rules.defaultThermalResistanceCPerW when neither is available - geometry unknown, never divide by zero or fabricate a number.
double estimateThermalResistanceCPerW(double aeMm2, double leMm, double coreWoundSurfaceAreaMm2, const DesignRules& rules, bool& isGeometryDerivedOut, bool& usesRealSurfaceAreaOut) {
    isGeometryDerivedOut = false;
    usesRealSurfaceAreaOut = false;
    if (rules.naturalConvectionCoefficientWPerM2K <= 0.0) {
        return rules.defaultThermalResistanceCPerW;
    }

    //this is the real Surface Area wound from the datasheet
    if (coreWoundSurfaceAreaMm2 > 0.0) {
        //convert mm^2 to m^2
        constexpr double kMm2ToM2 = 1e-6;
        double surfaceAreaM2 = coreWoundSurfaceAreaMm2 * kMm2ToM2;
        isGeometryDerivedOut = true;
        usesRealSurfaceAreaOut = true;
        //Rth = 1 / (h * publishedSurfaceArea from the datasheets) : note that h is our default coef which is 25
        return 1.0 / (rules.naturalConvectionCoefficientWPerM2K * surfaceAreaM2);
    }
    
    //check if ae or le are less than 0 meaning it has no values then we return the default thermal resistance
    if (aeMm2 <= 0.0 || leMm <= 0.0) {
        return rules.defaultThermalResistanceCPerW;
    }

    //none above: then we aree now going to use the fallback that estimates it from the magnetic core geometry: volume = (Ae x Le)
    constexpr double kMm3ToM3 = 1e-9;
    double volumeM3 = aeMm2 * leMm * kMm3ToM3;
    //estimate surface area from the volume
    double surfaceAreaM2 = rules.compactSolidSurfaceAreaShapeFactor * std::pow(volumeM3, 2.0 / 3.0);
    if (surfaceAreaM2 <= 0.0) {
        return rules.defaultThermalResistanceCPerW;
    }
    isGeometryDerivedOut = true;
    //still use the Thermal Resistance Formula but this has the fallback using Ae and Le
    return 1.0 / (rules.naturalConvectionCoefficientWPerM2K * surfaceAreaM2);
}

}  //namespace

//precondition: find the final winding temperature
//postcondition: see block comment above. Iterates up to rules.maxThermalIterations times (mirrors TurnsAndGapDesign.cpp's kMaxIterations pattern), converged when the winding-temperature change between
//iterations is below rules.thermalConvergenceThresholdC. Returns NotEvaluated (not a stale intermediate number) when copperLossGeometryKnown is false, or when the loop fails to converge within the iteration cap.
ThermalEvaluationResult evaluateThermal(const ThermalIterationInputs& inputs, const DesignRules& rules) {
    ThermalEvaluationResult result;

    //base check that verifies if we can calcualte DCR
    if (!inputs.copperLossGeometryKnown) {
        result.status = ThermalStatus::NotEvaluated;
        result.missingDataExplanation = "cannot seed a hot-DCR estimate without known winding DCR geometry (see WindingDesignResult::resistanceStatus)";
        return result;
    }

    bool thermalResistanceIsGeometryDerived = false;
    bool thermalResistanceUsesRealSurfaceArea = false;
    //getting the thermal resistance from above which tells us how many C per watt the device rises
    double thermalResistanceCPerWUsed = estimateThermalResistanceCPerW(inputs.coreEffectiveAreaMm2, inputs.coreMagneticPathLengthMm, inputs.coreWoundSurfaceAreaMm2, rules, thermalResistanceIsGeometryDerived, thermalResistanceUsesRealSurfaceArea);

    //this is the initial guess, winding is initially at ambient
    double windingTempC = inputs.ambientTemperatureC;
    double hotDcrOhms = inputs.coldDcrOhmsAt20C;
    double copperLossW = 0.0;
    double knownLossW = 0.0;
    double riseC = 0.0;
    bool converged = false;
    //declare loop variable earlier beause it will be used after the loop ends (result.iterationsUsed = iteration)
    int iteration = 0;

    for (; iteration < rules.maxThermalIterations; ++iteration) {
        //calculate hot resistance, copper resistance increases with temperature. Formula: Rhot = Rc x (1 + aDeltaT)
        hotDcrOhms = inputs.coldDcrOhmsAt20C * (1.0 + rules.copperTempCoefficientPerC * (windingTempC - 20.0));
        //Formula: copperLossW = I^2 x R
        copperLossW = inputs.rmsCurrentA * inputs.rmsCurrentA * hotDcrOhms;
        //add core loss. Formula: KnownLossW = CopperLossW + CoreLossW but if core loss was not solve then give it the value 0.0
        knownLossW = copperLossW + (inputs.coreLossKnown ? inputs.coreLossW : 0.0);
        //calculating temperature rise using Formula: DeltaT = P x Rth
        riseC = knownLossW * thermalResistanceCPerWUsed;
        //now calculate the new temperature using user ambient temp + the rise temp
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
        /*
        Genuine positive-feedback divergence (see header) for a high-current/low-DCR design is a real, reachable case here, not just a numerical edge case - the last computed intermediate temperature
        is not a meaningful estimate in that case, so it is not reported as one.
        */
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
    result.thermalResistanceUsesRealSurfaceArea = thermalResistanceUsesRealSurfaceArea;
    return result;
}
