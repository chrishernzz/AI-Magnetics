#include <cassert>
#include <cstdio>
#include "TestHelpers.h"
#include "core/magnetics/PermeabilityRolloff.h"
#include "data/DCBiasCurveDatabase.h"

namespace {

//real curve rows this suite exercises against, transcribed verbatim from Magnetics Inc./Micrometals'
//own published "Permeability vs. DC Bias" datasheet pages (Toroid shape family) - see data/dc_bias_curves.csv.
void loadRealCurves() {
    std::vector<DCBiasCurveData> rows;

    DCBiasCurveData koolMu60;
    koolMu60.materialName = "Kool Mu 60 test";
    koolMu60.a = 0.01;
    koolMu60.b = 2.142e-06;
    koolMu60.c = 1.855;
    koolMu60.d = 0.0;
    rows.push_back(koolMu60);

    DCBiasCurveData mix26;
    mix26.materialName = "Mix 26 test";
    mix26.a = 1.00e-02;
    mix26.b = 9.70e-06;
    mix26.c = 1.72;
    mix26.d = 0.00;
    rows.push_back(mix26);

    DCBiasCurveDatabase::setData(std::move(rows));
}

}  //namespace

//1. Cross-check against Magnetics Inc.'s own published worked example (their DC Inductor Design Guide's
//stated result): a 60mu Kool Mu core at H=57.5 Oe retains 72% of initial permeability. Confirms both the
//formula and the transcribed coefficients are correct, not just internally self-consistent.
void testPercentInitialPermeabilityMatchesKoolMu60DatasheetExample() {
    DCBiasCurveData curve;
    curve.a = 0.01;
    curve.b = 2.142e-06;
    curve.c = 1.855;
    curve.d = 0.0;

    double percent = percentInitialPermeability(57.5, curve);
    assert(approxEqual(percent, 72.0, 1.0));  //within 1 percentage point of the datasheet's stated 72%
    std::printf("testPercentInitialPermeabilityMatchesKoolMu60DatasheetExample: %.2f%% (datasheet says 72%%)\n", percent);
}

//2. Cross-check against Micrometals' own datasheet-stated spot values for Mix-26: "%Perm at DC Bias (50 Oe)
//= 55.2% (nom) / 47.4% (min)". Our transcribed coefficients (nominal fit) should land at/near the nominal value.
void testPercentInitialPermeabilityMatchesMix26DatasheetExample() {
    DCBiasCurveData curve;
    curve.a = 1.00e-02;
    curve.b = 9.70e-06;
    curve.c = 1.72;
    curve.d = 0.00;

    double percent = percentInitialPermeability(50.0, curve);
    assert(percent >= 40.0 && percent <= 60.0);  //brackets both the datasheet's min (47.4%) and nom (55.2%)
    std::printf("testPercentInitialPermeabilityMatchesMix26DatasheetExample: %.2f%% (datasheet: 55.2%% nom / 47.4%% min)\n", percent);
}

//3. At H=0 (no DC bias at all), every real curve in this dataset has a=0.01 (or 1.00E-02), so
//%mu = 1/0.01 = 100% exactly - the curve's own consistent "no bias yet" anchor point.
void testPercentInitialPermeabilityIsFullAtZeroBias() {
    DCBiasCurveData curve;
    curve.a = 0.01;
    curve.b = 1.165e-07;
    curve.c = 2.436;
    curve.d = 0.0;

    double percent = percentInitialPermeability(0.0, curve);
    assert(approxEqual(percent, 100.0, 1e-9));
    std::printf("testPercentInitialPermeabilityIsFullAtZeroBias: %.6f%% (exactly 100 as expected)\n", percent);
}

//4. The result is clamped to [0, 100] - never negative (b*H^c can only grow the denominator, so this proves
//the clamp exists rather than relying on the formula never overshooting on some future material's coefficients).
void testPercentInitialPermeabilityNeverExceeds100() {
    DCBiasCurveData curve;
    curve.a = 0.001;  //deliberately below every real material's 0.01, to probe the clamp
    curve.b = 1e-09;
    curve.c = 2.0;
    curve.d = 5.0;  //deliberately nonzero to push the raw formula above 100

    double percent = percentInitialPermeability(0.0, curve);
    assert(percent <= 100.0);
    std::printf("testPercentInitialPermeabilityNeverExceeds100: %.4f%% (clamped)\n", percent);
}

//5. H = 0.4*pi*N*I/le - verified against a simple hand-computed case (N=100, I=1A, le=10cm):
//H = 0.4*pi*100*1/10 = 4*pi ~= 12.566 Oe.
void testDcMagnetizingForceFormula() {
    double h = dcMagnetizingForceOe(100, 1.0, 100.0);  //le=100mm=10cm
    assert(approxEqual(h, 4.0 * 3.14159265358979323846, 1e-6));
    std::printf("testDcMagnetizingForceFormula: H=%.6f Oe (expected 4*pi=%.6f)\n", h, 4.0 * 3.14159265358979323846);
}

//6. Zero current is a real, valid input (e.g. a request with no operating current known yet) - must return
//H=0, not divide-by-zero or otherwise misbehave.
void testDcMagnetizingForceIsZeroAtZeroCurrent() {
    double h = dcMagnetizingForceOe(50, 0.0, 100.0);
    assert(approxEqual(h, 0.0, 1e-12));
    std::printf("testDcMagnetizingForceIsZeroAtZeroCurrent: H=%.6f Oe\n", h);
}

//7. findDCBiasCurve() must return found=true with the real row for a material that has one loaded.
void testFindDCBiasCurveReturnsRealRowForKnownMaterial() {
    loadRealCurves();
    DCBiasCurveLookup lookup = findDCBiasCurve("Kool Mu 60 test");
    assert(lookup.found);
    assert(approxEqual(lookup.curve.b, 2.142e-06, 1e-12));
    std::printf("testFindDCBiasCurveReturnsRealRowForKnownMaterial: found b=%.6e\n", lookup.curve.b);
}

//8. A material with no published curve in the dataset must report found=false, never a fabricated/default
//curve that would silently claim roll-off data this project doesn't actually have.
void testFindDCBiasCurveReportsNotFoundForUnknownMaterial() {
    loadRealCurves();
    DCBiasCurveLookup lookup = findDCBiasCurve("Definitely Not A Real Material 999");
    assert(!lookup.found);
    std::printf("testFindDCBiasCurveReportsNotFoundForUnknownMaterial: found=false as expected\n");
}

void runPermeabilityRolloffTests() {
    testPercentInitialPermeabilityMatchesKoolMu60DatasheetExample();
    testPercentInitialPermeabilityMatchesMix26DatasheetExample();
    testPercentInitialPermeabilityIsFullAtZeroBias();
    testPercentInitialPermeabilityNeverExceeds100();
    testDcMagnetizingForceFormula();
    testDcMagnetizingForceIsZeroAtZeroCurrent();
    testFindDCBiasCurveReturnsRealRowForKnownMaterial();
    testFindDCBiasCurveReportsNotFoundForUnknownMaterial();
    std::printf("All PermeabilityRolloffTests passed.\n");
}
