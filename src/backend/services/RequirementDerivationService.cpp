#include "RequirementDerivationService.h"
#include <cmath>
#include <stdexcept>

// precondition: request carries either rmsCurrentA, or both averageCurrentA
// and rippleCurrentPeakToPeakA
// postcondition: returns normalized InductorRequirements with converted
// units and, if applicable, a derived RMS current plus a stated assumption
InductorRequirements RequirementDerivationService::derive(const InductorDesignRequest& request, const DesignRules& rules) {
    InductorRequirements out;

    out.operatingPoint.inductanceH = request.inductanceUH * 1e-6;
    out.operatingPoint.peakCurrentA = request.peakCurrentA;
    out.operatingPoint.switchingFreqHz = request.switchingFreqKHz * 1000.0;

    if (request.rmsCurrentA.has_value()) {
        // Peak current and RMS current are never interchangeable (spec
        // section 5) - an explicitly supplied RMS current is always used
        // as-is, never inferred from peak current.
        out.operatingPoint.rmsCurrentA = *request.rmsCurrentA;
        out.operatingPoint.rmsCurrentDerived = false;
    } else if (request.averageCurrentA.has_value() && request.rippleCurrentPeakToPeakA.has_value()) {
        double iavg = *request.averageCurrentA;
        double ripple = *request.rippleCurrentPeakToPeakA;
        // Irms = sqrt(Iavg^2 + ripple^2/12), valid for a triangular ripple
        // waveform only.
        out.operatingPoint.rmsCurrentA = std::sqrt(iavg * iavg + (ripple * ripple) / 12.0);
        out.operatingPoint.rmsCurrentDerived = true;
        out.operatingPoint.rmsCurrentAssumption =
            "rmsCurrentA derived from averageCurrentA and rippleCurrentPeakToPeakA "
            "assuming a triangular ripple waveform (Irms = sqrt(Iavg^2 + ripple^2/12)). "
            "Supply rmsCurrentA directly if the real waveform is not triangular.";
    } else {
        throw std::invalid_argument(
            "rmsCurrentA is required (directly, or derivable from averageCurrentA + "
            "rippleCurrentPeakToPeakA for a triangular ripple waveform). Peak current "
            "cannot be used to infer RMS current.");
    }

    out.ambientTemperatureC = request.ambientTemperatureC;
    out.allowableTempRiseC = request.allowableTempRiseC;
    out.inductanceTolerancePercent =
        request.inductanceTolerancePercent.value_or(rules.defaultInductanceTolerancePercent);

    out.maximumDcrMilliOhm = request.maximumDcrMilliOhm;
    out.maximumWidthMm = request.maximumWidthMm;
    out.maximumHeightMm = request.maximumHeightMm;
    out.maximumLengthMm = request.maximumLengthMm;
    out.preferredMaterialFamily = request.preferredMaterialFamily;
    out.preferredCoreGeometry = request.preferredCoreGeometry;

    return out;
}
