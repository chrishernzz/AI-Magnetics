#include <cassert>
#include <cmath>
#include <cstdio>
#include "TestHelpers.h"
#include "core/thermal/ThermalEvaluation.h"

namespace {

ThermalIterationInputs baseInputs() {
    ThermalIterationInputs inputs;
    inputs.ambientTemperatureC = 25.0;
    inputs.rmsCurrentA = 10.0;
    inputs.coldDcrOhmsAt20C = 0.01;
    inputs.copperLossGeometryKnown = true;
    inputs.coreLossKnown = false;
    return inputs;
}

//1. Without known winding DCR geometry, the loop cannot even seed a hot-DCR estimate - must report
//NotEvaluated, never an invented starting point.
void testNotEvaluatedWhenGeometryUnknown() {
    DesignRules rules = DesignRules::phase1Default();
    ThermalIterationInputs inputs;
    inputs.copperLossGeometryKnown = false;

    ThermalEvaluationResult result = evaluateThermal(inputs, rules);
    assert(result.status == ThermalStatus::NotEvaluated);
    assert(!result.missingDataExplanation.empty());
    std::printf("testNotEvaluatedWhenGeometryUnknown: ok\n");
}

//2. A typical copper-loss-only design must converge to a real fixed point - cross-checked against an
//independent hand-simulation of the exact same iteration (ambient=25C, 10A, 0.01ohm cold DCR, 15 C/W Rth).
void testConvergesForTypicalCopperOnlyDesign() {
    DesignRules rules = DesignRules::phase1Default();
    ThermalIterationInputs inputs = baseInputs();

    ThermalEvaluationResult result = evaluateThermal(inputs, rules);
    assert(result.status == ThermalStatus::PreliminaryThermalEstimate);
    assert(result.converged);
    assert(result.iterationsUsed == 3);
    assert(approxEqual(result.convergedWindingTempC, 41.24952633646187, 1e-6));
    assert(approxEqual(result.hotDcrOhms, 0.01083301755764125, 1e-9));
    assert(approxEqual(result.copperLossAtConvergedTempW, 1.083301755764125, 1e-9));
    assert(approxEqual(result.knownLossW, result.copperLossAtConvergedTempW, 1e-9));  // no core loss known
    assert(approxEqual(result.predictedTempRiseC, 16.249526336461873, 1e-6));
    assert(approxEqual(result.predictedHotspotTempC, result.convergedWindingTempC, 1e-12));
    assert(approxEqual(result.thermalResistanceCPerWUsed, rules.defaultThermalResistanceCPerW, 1e-12));
    std::printf("testConvergesForTypicalCopperOnlyDesign: convergedWindingTempC=%.6f (%d iterations)\n",
                result.convergedWindingTempC, result.iterationsUsed);
}

//3. Including a known core loss must raise the converged temperature above the copper-only case, and
//knownLossW must equal copper loss plus the supplied core loss - cross-checked against an independent hand
//simulation of the same scenario with a real (nonzero) core loss added.
void testKnownCoreLossRaisesConvergedTemperature() {
    DesignRules rules = DesignRules::phase1Default();
    ThermalIterationInputs inputs = baseInputs();
    inputs.coreLossKnown = true;
    inputs.coreLossW = 0.3;

    ThermalEvaluationResult result = evaluateThermal(inputs, rules);
    assert(result.status == ThermalStatus::PreliminaryThermalEstimate);
    assert(approxEqual(result.convergedWindingTempC, 46.03043929771188, 1e-6));
    assert(approxEqual(result.knownLossW, 1.402029286514125, 1e-9));
    assert(approxEqual(result.knownLossW, result.copperLossAtConvergedTempW + 0.3, 1e-9));
    std::printf("testKnownCoreLossRaisesConvergedTemperature: convergedWindingTempC=%.6f (vs %.6f copper-only)\n",
                result.convergedWindingTempC, 41.24952633646187);
}

//4. A high-current, low-DCR design still within the loop's stable region must converge (more iterations,
//higher temperature, but a real fixed point) - cross-checked the same way as test 2.
void testConvergesForHighCurrentLowDcrStillWithinStableRegion() {
    DesignRules rules = DesignRules::phase1Default();
    ThermalIterationInputs inputs;
    inputs.ambientTemperatureC = 25.0;
    inputs.rmsCurrentA = 60.0;
    inputs.coldDcrOhmsAt20C = 0.002;
    inputs.copperLossGeometryKnown = true;
    inputs.coreLossKnown = false;

    ThermalEvaluationResult result = evaluateThermal(inputs, rules);
    assert(result.status == ThermalStatus::PreliminaryThermalEstimate);
    assert(result.converged);
    assert(result.iterationsUsed == 10);
    assert(approxEqual(result.convergedWindingTempC, 216.2942268634243, 1e-4));
    std::printf("testConvergesForHighCurrentLowDcrStillWithinStableRegion: convergedWindingTempC=%.4f (%d iterations)\n",
                result.convergedWindingTempC, result.iterationsUsed);
}

//5. Positive-feedback divergence (see ThermalEvaluation.h): for a high-enough current at a low-enough cold
//DCR, the loop's local gain (rmsCurrentA^2 * coldDcrOhmsAt20C * copperTempCoefficientPerC *
//defaultThermalResistanceCPerW) exceeds 1 and the iteration never stabilizes within the cap - confirmed by
//an independent hand-simulation of the identical formula reaching kMaxIterations without converging. This
//must report NotEvaluated, never a stale/meaningless intermediate number dressed up as an estimate.
void testDivergesHonestlyReportsNotEvaluatedRatherThanAStaleNumber() {
    DesignRules rules = DesignRules::phase1Default();
    ThermalIterationInputs inputs;
    inputs.ambientTemperatureC = 25.0;
    inputs.rmsCurrentA = 150.0;
    inputs.coldDcrOhmsAt20C = 0.002;
    inputs.copperLossGeometryKnown = true;
    inputs.coreLossKnown = false;

    // Confirm this scenario really does have loop gain >= 1 before trusting the "must not converge" assertion.
    double gain = inputs.rmsCurrentA * inputs.rmsCurrentA * inputs.coldDcrOhmsAt20C * rules.copperTempCoefficientPerC *
                  rules.defaultThermalResistanceCPerW;
    assert(gain >= 1.0);

    ThermalEvaluationResult result = evaluateThermal(inputs, rules);
    assert(result.status == ThermalStatus::NotEvaluated);
    assert(!result.converged);
    assert(!result.missingDataExplanation.empty());
    // Never silently populate the "preliminary estimate" fields when status says NotEvaluated.
    assert(approxEqual(result.convergedWindingTempC, 0.0, 1e-12));
    std::printf("testDivergesHonestlyReportsNotEvaluatedRatherThanAStaleNumber: gain=%.4f (>=1, diverges as expected), reason: %s\n",
                gain, result.missingDataExplanation.c_str());
}

}  // namespace

void runThermalTests() {
    testNotEvaluatedWhenGeometryUnknown();
    testConvergesForTypicalCopperOnlyDesign();
    testKnownCoreLossRaisesConvergedTemperature();
    testConvergesForHighCurrentLowDcrStillWithinStableRegion();
    testDivergesHonestlyReportsNotEvaluatedRatherThanAStaleNumber();
    std::printf("All ThermalTests passed.\n");
}
