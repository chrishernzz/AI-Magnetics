#include "RequirementDerivationService.h"
#include <cmath>
#include <stdexcept>

//precondition: request carries either rmsCurrentA, or both averageCurrentA and rippleCurrentPeakToPeakA
//postcondition: returns normalized InductorRequirements with converted units and, if applicable, a derived RMS current plus a stated assumption
InductorRequirements RequirementDerivationService::derive(const InductorDesignRequest& request, const DesignRules& rules) {
    InductorRequirements out;

    //the request supplies inductance in microhenries (uH) but the internal calculations use henries (H) - convert here so downstream stages never have to re-convert
    out.operatingPoint.inductanceH = request.inductanceUH * 1e-6;
    //no conversion needed here because the request supplies peak current in amps and the internal calculations use amps (will be used later for check such as peak flux density, core saturation, and energy storage)
    out.operatingPoint.peakCurrentA = request.peakCurrentA;
    //the request supplies switching frequency in kilohertz (kHz) but the internal calculations use hertz (Hz) - convert here so downstream stages never have to re-convert
    out.operatingPoint.switchingFreqHz = request.switchingFreqKHz * 1000.0;

    //if true meaning provided the RMS value then go in here (not nil)
    if (request.rmsCurrentA.has_value()) {
        //peak current and RMS current are never interchangeable (spec section 5) - an explicitly supplied RMS current is always used as-is, never inferred from peak current.
        out.operatingPoint.rmsCurrentA = *request.rmsCurrentA;
        out.operatingPoint.rmsCurrentDerived = false;
    } 
    //else if no provided value (nil) then calculate RMS current from average current and ripple current. Note: this is valid only for a triangular ripple waveform, which is the default assumption if the request does not supply rmsCurrentA directly.
    else if (request.averageCurrentA.has_value() && request.rippleCurrentPeakToPeakA.has_value()) {
        double iavg = *request.averageCurrentA;
        double ripple = *request.rippleCurrentPeakToPeakA;
        //Formula: Irms = sqrt(Iavg^2 + ripple^2/12), valid for a triangular ripple waveform only
        out.operatingPoint.rmsCurrentA = std::sqrt(iavg * iavg + (ripple * ripple) / 12.0);
        out.operatingPoint.rmsCurrentDerived = true;
        out.operatingPoint.rmsCurrentAssumption = "rmsCurrentA derived from averageCurrentA and rippleCurrentPeakToPeakA assuming a triangular ripple waveform (Irms = sqrt(Iavg^2 + ripple^2/12)). Supply rmsCurrentA directly if the real waveform is not triangular.";
    } 
    else {
        throw std::invalid_argument(
            "rmsCurrentA is required (directly, or derivable from averageCurrentA + rippleCurrentPeakToPeakA for a triangular ripple waveform). Peak current cannot be used to infer RMS current.");
    }

    //carried through independent of which branch above supplied rmsCurrentA - core loss needs the real ripple swing, not the derived RMS value
    out.operatingPoint.rippleCurrentPeakToPeakA = request.rippleCurrentPeakToPeakA;

    out.ambientTemperatureC = request.ambientTemperatureC;
    out.allowableTempRiseC = request.allowableTempRiseC;
    out.inductanceTolerancePercent = request.inductanceTolerancePercent.value_or(rules.defaultInductanceTolerancePercent);
    out.maximumDcrMilliOhm = request.maximumDcrMilliOhm;
    out.maximumWidthMm = request.maximumWidthMm;
    out.maximumHeightMm = request.maximumHeightMm;
    out.maximumLengthMm = request.maximumLengthMm;
    out.preferredMaterialFamily = request.preferredMaterialFamily;
    out.preferredCoreGeometry = request.preferredCoreGeometry;

    return out;
}
