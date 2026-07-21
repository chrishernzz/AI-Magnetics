#pragma once
#include <optional>
#include <string>

/*

STAGE 0: Normalized internal requirements
This is the C++-side counterpart of the API's InductorDesignRequest,after RequirementDerivationService has converted units (uH -> H, kHz ->
Hz) and derived any values that can be derived (e.g. rmsCurrentA from average current + ripple, for a triangular ripple waveform). Every stage
downstream of RequirementDerivationService reads from this struct, never
from the raw request, so there is exactly one place unit conversion and current-field derivation happens.

*/
struct OperatingPoint {
    double inductanceH;
    double peakCurrentA;
    double rmsCurrentA;
    double switchingFreqHz;

    //true if rmsCurrentA was derived from averageCurrentA + ripple rather
    //than supplied directly - callers must surface rmsCurrentAssumption
    //whenever this is true (spec section 5: "the system must state that
    //this relationship assumes triangular ripple").
    bool rmsCurrentDerived = false;
    std::string rmsCurrentAssumption;

    //Present only if the request supplied it directly - carried through
    //regardless of which branch rmsCurrentA came from, since core loss
    //(LossEvaluation) needs the real ripple swing, not the RMS value, to
    //compute flux-density swing. Never inferred from anything else.
    std::optional<double> rippleCurrentPeakToPeakA;
};

struct InductorRequirements {
    OperatingPoint operatingPoint;

    double ambientTemperatureC;
    double allowableTempRiseC;
    double inductanceTolerancePercent;

    //Optional inputs (spec section 5) - present only if the request supplied them. Nothing downstream may invent a value for these.
    std::optional<double> maximumDcrMilliOhm;
    std::optional<double> maximumWidthMm;
    std::optional<double> maximumHeightMm;
    std::optional<double> maximumLengthMm;
    std::optional<std::string> preferredMaterialFamily;
    std::optional<std::string> preferredCoreGeometry;
};
