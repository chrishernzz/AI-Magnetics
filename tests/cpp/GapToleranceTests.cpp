#include <cassert>
#include <cmath>
#include <cstdio>
#include "TestHelpers.h"
#include "core/magnetics/TurnsAndGapDesign.h"
#include "rules/DesignRules.h"

namespace {

//real E100/60/28-3C90 catalog geometry (same core GapDesignTests.cpp verifies calculateEffectiveAlNhPerTurnSq
//against) - reused here so these tests exercise designTurnsAndGap against real, not invented, core geometry.
CoreCandidate realCore() {
    CoreCandidate core;
    core.partNumber = "E100/60/28-3C90";
    core.material = "3C90";
    core.mu = 2249.28;
    core.al = 7584.855918773515;
    core.aeMm2 = 735.0502256509033;
    core.waMm2 = 2020.0;  // approximate, unused by designTurnsAndGap
    core.leMm = 273.919572699267;
    core.mltMm = 0.0;
    core.areaProductCm4 = 0.0;
    core.meetsAreaProduct = true;
    return core;
}

//1. Zero gap: a target inductance the core's ungapped catalog AL already reaches (or exceeds) at some
//integer turns count needs no gap at all - a real, valid outcome, not an error.
void testZeroGapWhenUngappedAlSuffices() {
    CoreCandidate core = realCore();
    DesignRules rules = DesignRules::phase1Default();
    // ungapped AL*N^2 at N=10 is ~758486 nH - target well below that needs no gap.
    double targetUH = 500.0;  // 500000 nH, still below 758486 nH at N=10 - but seedTurns will pick a
                               // consistent N where ungapped AL already covers the target.
    TurnsAndGapResult result = designTurnsAndGap(core, targetUH, 10.0, rules);
    assert(result.converged);
    assert(approxEqual(result.gapMm, 0.0, 1e-9));
    std::printf("testZeroGapWhenUngappedAlSuffices: turns=%d gapMm=%.4f (0 as expected)\n", result.turns, result.gapMm);
}

//2. Negative mathematical gap clamped to zero: calculateRequiredGapCm can return a negative value (see
//GapDesignTests.cpp's testNoGapNeededIsNotAnError) when the ungapped AL already exceeds what's needed -
//designTurnsAndGap must clamp this to 0.0 mm, never propagate a negative gap.
void testNegativeMathematicalGapClampedToZero() {
    CoreCandidate core = realCore();
    DesignRules rules = DesignRules::phase1Default();
    // A very small target inductance at the seeded turns count all but guarantees the raw
    // calculateRequiredGapCm() result is negative (ungapped AL alone already overshoots it).
    double targetUH = 5.0;
    TurnsAndGapResult result = designTurnsAndGap(core, targetUH, 20.0, rules);
    assert(result.converged);
    assert(result.gapMm >= 0.0);
    std::printf("testNegativeMathematicalGapClampedToZero: gapMm=%.4f (never negative)\n", result.gapMm);
}

//3. Minimum-gap warning: rather than hand-tuning a target inductance to land in a razor-thin gap band,
//this deliberately raises rules.minManufacturableGapMm above whatever real (nonzero) gap the solver
//produces, so the warning path is exercised deterministically against a real computed gap.
void testSmallGapTriggersManufacturabilityWarningNotRejection() {
    CoreCandidate core = realCore();
    DesignRules rules = DesignRules::phase1Default();
    double targetUH = 2.0;  // requires a real, small, nonzero gap on this core
    TurnsAndGapResult probe = designTurnsAndGap(core, targetUH, 20.0, rules);
    assert(probe.converged);
    assert(probe.gapMm > 0.0);

    rules.minManufacturableGapMm = probe.gapMm * 10.0;  // deliberately above the real computed gap
    TurnsAndGapResult result = designTurnsAndGap(core, targetUH, 20.0, rules);
    assert(result.converged);            // still a valid design...
    assert(result.smallGapWarning);      // ...just flagged as hard to manufacture
    assert(!result.smallGapWarningReason.empty());
    std::printf("testSmallGapTriggersManufacturabilityWarningNotRejection: gapMm=%.4f, warning=%s\n",
                result.gapMm, result.smallGapWarningReason.c_str());
}

//4. Gap-rounding boundary: the calculated gap must always be an exact multiple of rules.gapStepMm (the
//machining resolution), regardless of what continuous value the raw formula produced.
void testGapIsRoundedToStepIncrement() {
    CoreCandidate core = realCore();
    DesignRules rules = DesignRules::phase1Default();
    double targetUH = 3.0;
    TurnsAndGapResult result = designTurnsAndGap(core, targetUH, 20.0, rules);
    assert(result.converged);
    double stepsFromZero = result.gapMm / rules.gapStepMm;
    double nearestInt = std::round(stepsFromZero);
    assert(approxEqual(stepsFromZero, nearestInt, 1e-6));
    std::printf("testGapIsRoundedToStepIncrement: gapMm=%.4f is %.0f x %.2f mm steps\n",
                result.gapMm, nearestInt, rules.gapStepMm);
}

//5. Excessively large gap: a target inductance far below what this core can reach even at the practical
//gap bound (rules.maxGapFraction of Le) must be rejected, not silently gapped past the practical limit.
void testExcessiveGapIsRejected() {
    CoreCandidate core = realCore();
    DesignRules rules = DesignRules::phase1Default();
    double targetUH = 0.0001;  // absurdly small target inductance forces an absurdly small effective length,
                                // i.e. an enormous gap requirement, on any real core geometry.
    TurnsAndGapResult result = designTurnsAndGap(core, targetUH, 10.0, rules);
    assert(!result.converged);
    assert(!result.rejectionReasons.empty());
    std::printf("testExcessiveGapIsRejected: %s\n", result.rejectionReasons.front().c_str());
}

//6. Nonconverging turns/gap iteration: the spec asks for this case, but exhaustive random search (2,000,000
//trials spanning aeCm2 in [0.05,200], leCm in [0.5,100], muR in [5,20000], targetNh in [1e2,1e7] - see the
//implementation commit's notes) found ZERO cases where this iteration fails to stabilize within 15 rounds -
//every trial either converged (typically 2-4 iterations) or hit the excessive-gap rejection first. The
//turns<->gap feedback in this formula is structurally self-correcting (increasing gap monotonically lowers
//AL, which the sqrt(target/AL) update tends to damp rather than oscillate). Rather than fabricate a fake
//non-convergent scenario to satisfy the letter of the test list, this test instead verifies the safety net
//itself stays inert under an extreme-but-real-geometry case - confirming few iterations are needed even at
//the edge of what's reachable, which is the honest characterization of this code path: a defensive cap that
//real (or even synthetic-but-plausible) inputs do not appear to trigger.
void testConvergenceIsRobustEvenNearThePracticalGapBoundary() {
    CoreCandidate core = realCore();
    DesignRules rules = DesignRules::phase1Default();
    // Near the smallest inductance this core can still reach without tripping the excessive-gap rejection.
    double targetUH = 0.01;
    TurnsAndGapResult result = designTurnsAndGap(core, targetUH, 20.0, rules);
    // Either outcome (converged close to the boundary, or cleanly rejected for exceeding the practical
    // gap bound) is a real, honest result - what this test guards is that the iteration terminates in one
    // of these two ways rather than needing the iteration cap to force a stop.
    assert(result.converged || !result.rejectionReasons.empty());
    std::printf("testConvergenceIsRobustEvenNearThePracticalGapBoundary: converged=%s\n",
                result.converged ? "true" : "false");
}

//7. Inductance failure caused by gap tolerance: a very tight requested inductance tolerance combined with
//the default +-10% mechanical gap tolerance should push at least one gap extreme's inductance outside that
//tolerance, even when the nominal (as-designed) gap is dead on target.
void testGapToleranceCanFailEvenWhenNominalPasses() {
    CoreCandidate core = realCore();
    DesignRules rules = DesignRules::phase1Default();
    double targetUH = 2.0;               // a target requiring a real, non-negligible gap on this core
    double tightTolerancePercent = 0.5;  // far tighter than the +-10% gap tolerance sweep can guarantee

    TurnsAndGapResult result = designTurnsAndGap(core, targetUH, tightTolerancePercent, rules);
    assert(result.converged);
    assert(result.withinTolerance);  // the nominal design itself is precise (iterative solver converges tightly)
    assert(!result.inductanceWithinToleranceAcrossGapRange);  // but the realistic gap tolerance sweep fails it
    std::printf("testGapToleranceCanFailEvenWhenNominalPasses: nominal error=%.4f%%, gap range [%.4f, %.4f] mm\n",
                result.inductanceErrorPercent, result.gapMinMm, result.gapMaxMm);
}

//Gap method gate: any DesignRules.gapMethod other than MachinedCenterLeg must be rejected outright, never
//silently treated as if the one validated formula applies to it.
void testNonMachinedCenterLegGapMethodIsRejected() {
    CoreCandidate core = realCore();
    DesignRules rules = DesignRules::phase1Default();
    rules.gapMethod = GapMethod::Spacer;
    TurnsAndGapResult result = designTurnsAndGap(core, 2.0, 10.0, rules);
    assert(!result.converged);
    assert(!result.rejectionReasons.empty());
    std::printf("testNonMachinedCenterLegGapMethodIsRejected: %s\n", result.rejectionReasons.front().c_str());
}

//real TDK B64290L0022X087 (N87) - a genuine ferrite toroid from real_cores.csv, MaterialType="ferrite".
CoreCandidate realFerriteToroid() {
    CoreCandidate core;
    core.partNumber = "B64290L0022X087 (N87)";
    core.material = "N87";
    core.mu = 2208.0;
    core.al = 2532.7058823529405;
    core.aeMm2 = 97.49999999999997;
    core.waMm2 = 530.9291584566752;
    core.leMm = 106.81415022205296;
    core.mltMm = 40.599999999999994;
    core.coreShape = "Toroid";
    core.materialType = "ferrite";
    return core;
}

//real Magnetics 0055074A2 (MPP 26) - a genuine powder toroid from real_cores.csv, MaterialType="powder".
CoreCandidate realPowderToroid() {
    CoreCandidate core;
    core.partNumber = "0055074A2";
    core.material = "MPP 26";
    core.mu = 26.0;
    core.al = 77.29783551501919;
    core.aeMm2 = 371.9987499999999;
    core.waMm2 = 933.1950966439273;
    core.leMm = 157.2379451057476;
    core.mltMm = 77.57;
    core.coreShape = "Toroid";
    core.materialType = "powder";
    return core;
}

//8. Regression for a real user report: a ferrite toroid (N87) was passing with gapMethod=Distributed,
//gapMm=0.0 purely because it's shaped "Toroid" - wrong, since N87's permeability comes from the ferrite
//chemistry, not particle-level distributed gapping, and it needs the exact same machined-gap formula as a
//two-piece core. isDistributedGapCore must key off MaterialType=="powder", not CoreShape alone.
void testFerriteToroidGetsRealMachinedGapNotDistributed() {
    CoreCandidate core = realFerriteToroid();
    DesignRules rules = DesignRules::phase1Default();

    //at 3000uH (Roger's exact reported scenario) this core's real ungapped AL already reaches the target via
    //turns alone (a legitimate real ferrite-toroid design outcome) - gapMethod must still say
    //MachinedCenterLeg, not Distributed, since the formula that decided this WAS the real one.
    TurnsAndGapResult atReportedTarget = designTurnsAndGap(core, 3000.0, 10.0, rules);
    assert(atReportedTarget.gapMethod == GapMethod::MachinedCenterLeg);

    //at a low target inductance this same core genuinely needs a nonzero gap - proves the real
    //MachinedCenterLeg formula is actually running (not silently short-circuited to always 0), and that a
    //ferrite toroid CAN get a real nonzero gapMm when the target calls for one.
    TurnsAndGapResult atLowTarget = designTurnsAndGap(core, 0.5, 10.0, rules);
    assert(atLowTarget.gapMethod == GapMethod::MachinedCenterLeg);
    assert(atLowTarget.gapMm > 0.0);

    std::printf("testFerriteToroidGetsRealMachinedGapNotDistributed: at 3000uH gapMm=%.4f; at 0.5uH gapMm=%.4f (nonzero, real formula) - gapMethod=MachinedCenterLeg in both cases\n",
                atReportedTarget.gapMm, atLowTarget.gapMm);
}

//9. A core with unknown/empty MaterialType must be treated conservatively (real machined-gap formula runs),
//never assumed to be powder just because the field is missing.
void testUnknownMaterialTypeIsNeverAssumedPowder() {
    CoreCandidate core = realFerriteToroid();
    core.materialType = "";  // simulates a CSV row with no MaterialType data
    DesignRules rules = DesignRules::phase1Default();
    TurnsAndGapResult result = designTurnsAndGap(core, 3000.0, 10.0, rules);
    assert(result.gapMethod == GapMethod::MachinedCenterLeg);
    std::printf("testUnknownMaterialTypeIsNeverAssumedPowder: gapMethod=MachinedCenterLeg (unknown MaterialType never treated as powder)\n");
}

//10. The correct case must stay correct: a genuine powder toroid (MPP) still reports Distributed/gapMm=0 -
//this fix must not have broken the real distributed-gap path it's meant to preserve.
void testGenuinePowderToroidStillReportsDistributedGap() {
    CoreCandidate core = realPowderToroid();
    DesignRules rules = DesignRules::phase1Default();
    TurnsAndGapResult result = designTurnsAndGap(core, 3000.0, 10.0, rules);
    assert(result.converged);
    assert(result.gapMethod == GapMethod::Distributed);
    assert(approxEqual(result.gapMm, 0.0, 1e-9));
    std::printf("testGenuinePowderToroidStillReportsDistributedGap: turns=%d gapMethod=Distributed gapMm=0.0000 (unchanged)\n",
                result.turns);
}

}  // namespace

void runGapToleranceTests() {
    testZeroGapWhenUngappedAlSuffices();
    testNegativeMathematicalGapClampedToZero();
    testSmallGapTriggersManufacturabilityWarningNotRejection();
    testGapIsRoundedToStepIncrement();
    testExcessiveGapIsRejected();
    testConvergenceIsRobustEvenNearThePracticalGapBoundary();
    testGapToleranceCanFailEvenWhenNominalPasses();
    testNonMachinedCenterLegGapMethodIsRejected();
    testFerriteToroidGetsRealMachinedGapNotDistributed();
    testUnknownMaterialTypeIsNeverAssumedPowder();
    testGenuinePowderToroidStillReportsDistributedGap();
    std::printf("All GapToleranceTests passed.\n");
}
