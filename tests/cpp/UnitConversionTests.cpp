#include <cassert>
#include <cstdio>
#include "TestHelpers.h"
#include "core/units/UnitConversions.h"

namespace {
//precondition: none
//postcondition: every units:: function produces the exact expected value to very tight tolerance (1e-9 to 1e-12) -
//calls the real conversion functions used throughout the codebase, not a re-derivation of the same arithmetic in
//isolation, so this actually confirms the functions calculation modules call are correct.
void testUnitConversions() {
    //actual, expected, tolerance
    assert(approxEqual(units::uHToH(250.0), 0.00025, 1e-12));
    assert(approxEqual(units::hToUH(0.00025), 250.0, 1e-9));
    assert(approxEqual(units::kHzToHz(100.0), 100000.0, 1e-9));
    assert(approxEqual(units::hzToKHz(100000.0), 100.0, 1e-9));
    assert(approxEqual(units::mmToM(273.919572699267), 0.273919572699267, 1e-9));
    assert(approxEqual(units::mToMm(0.273919572699267), 273.919572699267, 1e-9));
    assert(approxEqual(units::mm2ToM2(735.0502256509033), 0.0007350502256509033, 1e-9));
    assert(approxEqual(units::m2ToMm2(0.0007350502256509033), 735.0502256509033, 1e-6));
    assert(approxEqual(units::mm3ToM3(1000.0), 1e-6, 1e-12));
    assert(approxEqual(units::m3ToMm3(1e-6), 1000.0, 1e-9));
    assert(approxEqual(units::milliOhmToOhm(500.0), 0.5, 1e-12));
    assert(approxEqual(units::ohmToMilliOhm(0.5), 500.0, 1e-9));
    assert(approxEqual(units::aPerCm2ToAPerMm2(400.0), 4.0, 1e-12));
    assert(approxEqual(units::aPerMm2ToAPerCm2(4.0), 400.0, 1e-9));

    //CGS domain (GapDesign/TurnsAndGapDesign/TurnsCalculation)
    assert(approxEqual(units::mmToCm(273.919572699267), 27.3919572699267, 1e-9));
    assert(approxEqual(units::cmToMm(27.3919572699267), 273.919572699267, 1e-9));
    assert(approxEqual(units::mm2ToCm2(735.0502256509033), 7.350502256509033, 1e-9));
    assert(approxEqual(units::uHToNh(2.4), 2400.0, 1e-9));
    assert(approxEqual(units::nHToUh(2400.0), 2.4, 1e-9));

    std::printf("testUnitConversions: ok\n");
}
} // namespace


void runUnitConversionTests() {
    testUnitConversions();
    std::printf("All UnitConversionTests passed.\n");
}
