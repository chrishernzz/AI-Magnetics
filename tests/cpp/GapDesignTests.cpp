#include <cassert>
#include <cstdio>
#include "TestHelpers.h"
#include "core/magnetics/GapDesign.h"

namespace {
//precondition: aeCm2, leCm, muR are the real ungapped catalog values from E100/60/28-3C90 (Converted from the datasheet's mm/mm^2 units into cm/cm^2 units as calculateEffectiveAlNhPerTurnSq expects). gapCm is 0.0 (ungapped)
//postcondition: calculateEffectiveAlNhPerTurnSq must return the AL value the manufacturer publishes for this core (within 5 nH/turn^2). If the assert fails, the process aborts here and no later test runs
void testUngappedAlMatchesCatalog() {
    double aeCm2 = 735.0502256509033 / 100.0;
    double leCm = 273.919572699267 / 10.0;
    double muR = 2249.28;

    double actualAl = calculateEffectiveAlNhPerTurnSq(aeCm2, leCm, muR, 0.0);
    //check if its within the tolerance
    assert(approxEqual(actualAl, 7584.855918773515, 5.0));
    std::printf("testUngappedAlMatchesCatalog: AL=%.3f (expected ~7584.856)\n", actualAl);
}

//precondition: aeCm2, leCm, muR describe a real core geometry; turns > 0; targetNh is an inductance target that requires a nonzero gap to reach at this turns count (i.e.below the ungapped AL * N^2 for this core)
/*postcondition: calculateRequiredGapCm must return a strictly positive gap (asserted first), and feeding that gap back into calculateEffectiveAlNhPerTurnSq must reproduce targetNh (within 1 nH) 
//once multiplied by turns^2 - i.e. the two GapDesing functions are inverses of each other for this input. Either assert failing aborts the process*/
void testGapRoundTrip() {
    double aeCm2 = 7.350502256509033;
    double leCm = 27.3919572699267;
    double muR = 2249.28;
    int turns = 30;
    double targetNh = 5000.0;

    double gapCm = calculateRequiredGapCm(turns, aeCm2, leCm, muR, targetNh);
    //this target/turns combination should require a real gap
    assert(gapCm > 0.0);  

    double alEff = calculateEffectiveAlNhPerTurnSq(aeCm2, leCm, muR, gapCm);
    double actualNh = static_cast<double>(turns) * turns * alEff;

    assert(approxEqual(actualNh, targetNh, 1.0));
    std::printf("testGapRoundTrip: gap=%.4f cm, actualL=%.2f nH (target %.2f)\n", gapCm, actualNh, targetNh);
}

//precondition: aeCm2, leCm, muR describe a real core; turns = 1; targetNh (50000) is chosen to be far above the ungapped inductance thhis core can produce at N = 1 (~7585), so no gap can reach it - more turns would be needed instead
//postcondition: calculateRequiredGapCm must return a zero or negative gap <= 0.0 signal "unreachable via gapping alone" rather than throwing or returning a nonsensical positive gap. This is documented non-error return value, not an exception path
void testNoGapNeededIsNotAnError() {
    double aeCm2 = 7.350502256509033;
    double leCm = 27.3919572699267;
    double muR = 2249.28;
    int turns = 1;
    //far above the ungapped L at N=1 (~7585 nH)
    double targetNh = 50000.0; 

    double gapCm = calculateRequiredGapCm(turns, aeCm2, leCm, muR, targetNh);
    assert(gapCm <= 0.0);
    std::printf("testNoGapNeededIsNotAnError: gap=%.4f cm (negative/zero as expected)\n", gapCm);
}
} // namespace

void runGapDesignTests() {
    testUngappedAlMatchesCatalog();
    testGapRoundTrip();
    testNoGapNeededIsNotAnError();
    std::printf("All GapDesignTests passed.\n");
}