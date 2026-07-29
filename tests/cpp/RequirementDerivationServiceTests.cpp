#include <cassert>
#include <cmath>
#include <cstdio>
#include "TestHelpers.h"
#include "backend/services/RequirementDerivationService.h"
#include "rules/DesignRules.h"

namespace {

//precondition: none
//postcondition: a minimal valid request - inductanceUH/switchingFreqKHz/ambient/allowableRise are
//required by every real request regardless of which current fields this test cares about.
InductorDesignRequest baseRequest() {
    InductorDesignRequest req{};
    req.inductanceUH = 100.0;
    req.switchingFreqKHz = 100.0;
    req.ambientTemperatureC = 25.0;
    req.allowableTempRiseC = 40.0;
    return req;
}

//1. RMS+ripple, no peak, a real triangular-waveform-consistent combination (Irms^2 >= ripple^2/12) -
//peak must be derived, flagged, and land in CCM.
void testDerivesPeakFromRmsAndRippleWhenConsistent() {
    InductorDesignRequest req = baseRequest();
    req.rmsCurrentA = 5.0;
    req.rippleCurrentPeakToPeakA = 2.0;
    DesignRules rules = DesignRules::phase1Default();

    InductorRequirements result = RequirementDerivationService::derive(req, rules);

    assert(result.operatingPoint.peakCurrentA.has_value());
    assert(result.operatingPoint.peakCurrentDerived);
    assert(!result.operatingPoint.peakCurrentAssumption.empty());

    //hand-verify: Iavg = sqrt(5^2 - 2^2/12) = sqrt(25 - 0.3333) = sqrt(24.6667) = 4.9666
    double expectedIavg = std::sqrt(5.0 * 5.0 - (2.0 * 2.0) / 12.0);
    double expectedPeak = expectedIavg + 2.0 / 2.0;
    assert(approxEqual(*result.operatingPoint.peakCurrentA, expectedPeak, 1e-6));
    assert(result.operatingPoint.currentConsistencyStatus == EvaluationStatus::Evaluated);
    assert(result.operatingPoint.conductionMode == ConductionMode::CCM);
    std::printf("testDerivesPeakFromRmsAndRippleWhenConsistent: peak=%.4f A (expected %.4f)\n",
                *result.operatingPoint.peakCurrentA, expectedPeak);
}

//2. Same idea, but ripple large enough relative to RMS that the derived minInductorCurrentA goes
//negative - DCM implied. peakCurrentA must still be populated (mirrors the existing peak+ripple
//branch's philosophy of not clearing peak on a DCM finding).
void testDerivedPeakStillPopulatedWhenImpliesDcm() {
    InductorDesignRequest req = baseRequest();
    req.rmsCurrentA = 3.0;
    req.rippleCurrentPeakToPeakA = 8.0;
    DesignRules rules = DesignRules::phase1Default();

    InductorRequirements result = RequirementDerivationService::derive(req, rules);

    //Irms^2 (9) vs ripple^2/12 (64/12=5.333) - 9 >= 5.333, derivation proceeds
    assert(result.operatingPoint.peakCurrentA.has_value());
    assert(result.operatingPoint.peakCurrentDerived);
    assert(result.operatingPoint.currentConsistencyStatus == EvaluationStatus::NotEvaluated);
    assert(result.operatingPoint.conductionMode == ConductionMode::DCMUnsupported);
    std::printf("testDerivedPeakStillPopulatedWhenImpliesDcm: peak=%.4f A, minInductorCurrentA=%.4f A (DCM implied)\n",
                *result.operatingPoint.peakCurrentA, result.operatingPoint.minInductorCurrentA);
}

//3. Boundary: Irms^2 == ripple^2/12 exactly (Iavg = 0) - must not crash on sqrt(0), derives a real peak.
void testBoundaryIavgZero() {
    InductorDesignRequest req = baseRequest();
    double ripple = 4.0;
    double irms = ripple / std::sqrt(12.0);  // makes Irms^2 exactly ripple^2/12
    req.rmsCurrentA = irms;
    req.rippleCurrentPeakToPeakA = ripple;
    DesignRules rules = DesignRules::phase1Default();

    InductorRequirements result = RequirementDerivationService::derive(req, rules);

    assert(result.operatingPoint.peakCurrentA.has_value());
    assert(result.operatingPoint.peakCurrentDerived);
    assert(approxEqual(*result.operatingPoint.peakCurrentA, ripple / 2.0, 1e-6));
    std::printf("testBoundaryIavgZero: peak=%.4f A (expected %.4f, Iavg=0 boundary)\n",
                *result.operatingPoint.peakCurrentA, ripple / 2.0);
}

//4. Failure: Irms^2 < ripple^2/12 - physically impossible for a triangular waveform. Must not throw;
//peakCurrentA stays absent, and the explanation cites the real numbers.
void testDerivationFailsHonestlyWhenPhysicallyInconsistent() {
    InductorDesignRequest req = baseRequest();
    req.rmsCurrentA = 1.0;
    req.rippleCurrentPeakToPeakA = 10.0;  // ripple^2/12 = 8.333 > Irms^2 = 1.0
    DesignRules rules = DesignRules::phase1Default();

    InductorRequirements result = RequirementDerivationService::derive(req, rules);

    assert(!result.operatingPoint.peakCurrentA.has_value());
    assert(!result.operatingPoint.peakCurrentDerived);
    assert(!result.operatingPoint.currentConsistencyExplanation.empty());
    std::printf("testDerivationFailsHonestlyWhenPhysicallyInconsistent: %s\n",
                result.operatingPoint.currentConsistencyExplanation.c_str());
}

//5. Round-trip: averageCurrentA + ripple supplied (no rmsCurrentA, no peak) - RMS gets derived first
//(Mode 1 derivation), then peak gets derived from that SAME rms+ripple pair. The algebraic identity
//means the round-trip must land exactly back on the original average: peak == iavg + ripple/2.
void testRoundTripFromAverageAndRippleMatchesOriginalAverage() {
    InductorDesignRequest req = baseRequest();
    double iavg = 6.0;
    double ripple = 3.0;
    req.averageCurrentA = iavg;
    req.rippleCurrentPeakToPeakA = ripple;
    DesignRules rules = DesignRules::phase1Default();

    InductorRequirements result = RequirementDerivationService::derive(req, rules);

    assert(result.operatingPoint.rmsCurrentDerived);
    assert(result.operatingPoint.peakCurrentA.has_value());
    assert(result.operatingPoint.peakCurrentDerived);
    assert(approxEqual(*result.operatingPoint.peakCurrentA, iavg + ripple / 2.0, 1e-6));
    std::printf("testRoundTripFromAverageAndRippleMatchesOriginalAverage: peak=%.4f A (expected %.4f)\n",
                *result.operatingPoint.peakCurrentA, iavg + ripple / 2.0);
}

//6. Peak already directly supplied - the new derivation code must never run (regression guard against
//accidentally overwriting a real supplied value).
void testDirectlySuppliedPeakNeverOverwritten() {
    InductorDesignRequest req = baseRequest();
    req.rmsCurrentA = 5.0;
    req.peakCurrentA = 9.0;
    req.rippleCurrentPeakToPeakA = 2.0;
    DesignRules rules = DesignRules::phase1Default();

    InductorRequirements result = RequirementDerivationService::derive(req, rules);

    assert(result.operatingPoint.peakCurrentA.has_value());
    assert(approxEqual(*result.operatingPoint.peakCurrentA, 9.0, 1e-9));
    assert(!result.operatingPoint.peakCurrentDerived);
    assert(result.operatingPoint.peakCurrentAssumption.empty());
    std::printf("testDirectlySuppliedPeakNeverOverwritten: peak=%.4f A (unchanged, not derived)\n",
                *result.operatingPoint.peakCurrentA);
}

//7. RMS-only, no ripple at all - no derivation attempted, peak stays absent.
void testNoRippleMeansNoDerivationAttempted() {
    InductorDesignRequest req = baseRequest();
    req.rmsCurrentA = 5.0;
    DesignRules rules = DesignRules::phase1Default();

    InductorRequirements result = RequirementDerivationService::derive(req, rules);

    assert(!result.operatingPoint.peakCurrentA.has_value());
    assert(!result.operatingPoint.peakCurrentDerived);
    std::printf("testNoRippleMeansNoDerivationAttempted: peakCurrentA absent, as expected\n");
}

//8. Property check: whenever the derivation succeeds, peak_derived >= irms always holds (proven
//algebraically in RequirementDerivationService.cpp) - swept across several RMS/ripple combinations to
//confirm the existing "rms > peak" hard-throw sanity check can never fire on a derived peak.
void testDerivedPeakNeverLessThanRms() {
    DesignRules rules = DesignRules::phase1Default();
    double rmsRippleCombos[][2] = {
        {5.0, 2.0}, {10.0, 1.0}, {2.0, 3.0}, {50.0, 20.0}, {1.0, 0.5},
    };
    for (auto& combo : rmsRippleCombos) {
        //skip combos where derivation is expected to fail (Irms^2 < ripple^2/12)
        if (combo[0] * combo[0] < (combo[1] * combo[1]) / 12.0) {
            continue;
        }
        InductorDesignRequest req = baseRequest();
        req.rmsCurrentA = combo[0];
        req.rippleCurrentPeakToPeakA = combo[1];
        InductorRequirements result = RequirementDerivationService::derive(req, rules);
        assert(result.operatingPoint.peakCurrentA.has_value());
        assert(*result.operatingPoint.peakCurrentA >= combo[0] - 1e-9);
    }
    std::printf("testDerivedPeakNeverLessThanRms: property holds across all swept combinations\n");
}

}  // namespace

void runRequirementDerivationServiceTests() {
    testDerivesPeakFromRmsAndRippleWhenConsistent();
    testDerivedPeakStillPopulatedWhenImpliesDcm();
    testBoundaryIavgZero();
    testDerivationFailsHonestlyWhenPhysicallyInconsistent();
    testRoundTripFromAverageAndRippleMatchesOriginalAverage();
    testDirectlySuppliedPeakNeverOverwritten();
    testNoRippleMeansNoDerivationAttempted();
    testDerivedPeakNeverLessThanRms();
    std::printf("All RequirementDerivationServiceTests passed.\n");
}
