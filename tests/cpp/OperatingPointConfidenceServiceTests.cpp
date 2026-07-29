#include <cassert>
#include <cstdio>
#include "backend/services/OperatingPointConfidenceService.h"

namespace {

OperatingPoint baseOperatingPoint() {
    OperatingPoint op{};
    op.inductanceH = 1e-4;
    op.rmsCurrentA = 5.0;
    op.switchingFreqHz = 100000.0;
    return op;
}

//1. RmsOnly: no peak, no ripple.
void testRmsOnly() {
    OperatingPoint op = baseOperatingPoint();
    OperatingPointConfidence result = classifyOperatingPointConfidence(op);
    assert(result.inputMode == OperatingPointInputMode::RmsOnly);
    assert(result.peakCurrentProvenance == PeakCurrentProvenance::NotSupplied);
    assert(!result.summary.empty());
    std::printf("testRmsOnly: %s\n", result.summary.c_str());
}

//2. RmsPlusRipple with a successful derivation (peakCurrentDerived=true).
void testRmsPlusRippleWithSuccessfulDerivation() {
    OperatingPoint op = baseOperatingPoint();
    op.rippleCurrentPeakToPeakA = 2.0;
    op.peakCurrentA = 5.5;
    op.peakCurrentDerived = true;
    OperatingPointConfidence result = classifyOperatingPointConfidence(op);
    assert(result.inputMode == OperatingPointInputMode::RmsPlusRipple);
    assert(result.peakCurrentProvenance == PeakCurrentProvenance::Derived);
    std::printf("testRmsPlusRippleWithSuccessfulDerivation: %s\n", result.summary.c_str());
}

//3. RmsPlusRipple where derivation failed (peakCurrentA stays absent).
void testRmsPlusRippleWithFailedDerivation() {
    OperatingPoint op = baseOperatingPoint();
    op.rippleCurrentPeakToPeakA = 20.0;
    // peakCurrentA left absent - derivation failed
    OperatingPointConfidence result = classifyOperatingPointConfidence(op);
    assert(result.inputMode == OperatingPointInputMode::RmsPlusRipple);
    assert(result.peakCurrentProvenance == PeakCurrentProvenance::NotSupplied);
    std::printf("testRmsPlusRippleWithFailedDerivation: %s\n", result.summary.c_str());
}

//4. RmsPlusPeak: peak directly supplied, no ripple.
void testRmsPlusPeak() {
    OperatingPoint op = baseOperatingPoint();
    op.peakCurrentA = 7.0;
    op.peakCurrentDerived = false;
    OperatingPointConfidence result = classifyOperatingPointConfidence(op);
    assert(result.inputMode == OperatingPointInputMode::RmsPlusPeak);
    assert(result.peakCurrentProvenance == PeakCurrentProvenance::DirectlySupplied);
    std::printf("testRmsPlusPeak: %s\n", result.summary.c_str());
}

//5. FullOperatingPoint: peak AND ripple both directly supplied.
void testFullOperatingPoint() {
    OperatingPoint op = baseOperatingPoint();
    op.peakCurrentA = 7.0;
    op.peakCurrentDerived = false;
    op.rippleCurrentPeakToPeakA = 2.0;
    OperatingPointConfidence result = classifyOperatingPointConfidence(op);
    assert(result.inputMode == OperatingPointInputMode::FullOperatingPoint);
    assert(result.peakCurrentProvenance == PeakCurrentProvenance::DirectlySupplied);
    std::printf("testFullOperatingPoint: %s\n", result.summary.c_str());
}

//6. The critical subtlety: a successfully-derived peak (Mode 2) must NEVER be misclassified as
//RmsPlusPeak or FullOperatingPoint just because peakCurrentA.has_value() is now true.
void testDerivedPeakNeverMisclassifiedAsDirectlySupplied() {
    OperatingPoint op = baseOperatingPoint();
    op.rippleCurrentPeakToPeakA = 2.0;
    op.peakCurrentA = 5.5;
    op.peakCurrentDerived = true;

    OperatingPointConfidence result = classifyOperatingPointConfidence(op);

    assert(result.inputMode != OperatingPointInputMode::RmsPlusPeak);
    assert(result.inputMode != OperatingPointInputMode::FullOperatingPoint);
    assert(result.inputMode == OperatingPointInputMode::RmsPlusRipple);
    assert(result.peakCurrentProvenance == PeakCurrentProvenance::Derived);
    std::printf("testDerivedPeakNeverMisclassifiedAsDirectlySupplied: inputMode correctly stayed RmsPlusRipple despite peakCurrentA.has_value()\n");
}

}  // namespace

void runOperatingPointConfidenceServiceTests() {
    testRmsOnly();
    testRmsPlusRippleWithSuccessfulDerivation();
    testRmsPlusRippleWithFailedDerivation();
    testRmsPlusPeak();
    testFullOperatingPoint();
    testDerivedPeakNeverMisclassifiedAsDirectlySupplied();
    std::printf("All OperatingPointConfidenceServiceTests passed.\n");
}
