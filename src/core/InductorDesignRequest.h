#pragma once
#include <optional>
#include <string>

// Direct-entry inductor design request (spec section 6: renamed from the
// legacy BuckInput, which described this as a buck-specific input even
// though every field here is a direct inductor specification with no
// topology knowledge - no Buck/Boost fields belong on this struct).
struct InductorDesignRequest {
    double inductanceUH;
    double peakCurrentA;
    double switchingFreqKHz;
    double ambientTemperatureC;
    double allowableTempRiseC;

    // Optional because it can instead be derived from averageCurrentA +
    // rippleCurrentPeakToPeakA for a triangular ripple waveform - see
    // RequirementDerivationService. Peak current is never used to infer this.
    std::optional<double> rmsCurrentA;

    // If omitted, DesignRules::defaultInductanceTolerancePercent applies.
    std::optional<double> inductanceTolerancePercent;

    // Optional direct inputs (spec section 5)
    std::optional<double> averageCurrentA;
    std::optional<double> rippleCurrentPeakToPeakA;
    std::optional<double> maximumDcrMilliOhm;
    std::optional<double> maximumWidthMm;
    std::optional<double> maximumHeightMm;
    std::optional<double> maximumLengthMm;
    std::optional<std::string> preferredMaterialFamily;
    std::optional<std::string> preferredCoreGeometry;
};
