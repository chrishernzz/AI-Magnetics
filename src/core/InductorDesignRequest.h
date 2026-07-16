#pragma once
#include <optional>
#include <string>

/*

Directy-entry inductor design request struct, as recieved from the API. 
This is what is being input by user in the API request, and will be used to derive the InductorRequirements struct, 
which is the normalized internal requirements struct that will be used throughout the design pipeline.

*/
struct InductorDesignRequest {
    double inductanceUH;
    double peakCurrentA;
    double switchingFreqKHz;
    double ambientTemperatureC;
    double allowableTempRiseC;

    /*Optional because it can instead be derived from averageCurrentA +
      rippleCurrentPeakToPeakA for a triangular ripple waveform - see
      RequirementDerivationService. Peak current is never used to infer this.*/
    std::optional<double> rmsCurrentA;

    //If omitted, DesignRules::defaultInductanceTolerancePercent applies.
    std::optional<double> inductanceTolerancePercent;

    //Optional direct inputs (spec section 5)
    std::optional<double> averageCurrentA;
    std::optional<double> rippleCurrentPeakToPeakA;
    std::optional<double> maximumDcrMilliOhm;
    std::optional<double> maximumWidthMm;
    std::optional<double> maximumHeightMm;
    std::optional<double> maximumLengthMm;
    std::optional<std::string> preferredMaterialFamily;
    std::optional<std::string> preferredCoreGeometry;
};
