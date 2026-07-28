#include "RequirementDerivationService.h"
#include <cmath>
#include <stdexcept>
#include "core/units/UnitConversions.h"

//precondition: request carries either rmsCurrentA, or both averageCurrentA and rippleCurrentPeakToPeakA
//postcondition: returns normalized InductorRequirements with converted units and, if applicable, a derived RMS current plus a stated assumption
InductorRequirements RequirementDerivationService::derive(const InductorDesignRequest& request, const DesignRules& rules) {
    InductorRequirements out;

    //the request supplies inductance in microhenries (uH) but the internal calculations use henries (H) - convert here so downstream stages never have to re-convert
    out.operatingPoint.inductanceH = units::uHToH(request.inductanceUH);
    //no conversion needed here because the request supplies peak current in amps and the internal calculations use amps (will be used later for check such as peak flux density, core saturation, and energy storage)
    out.operatingPoint.peakCurrentA = request.peakCurrentA;
    //the request supplies switching frequency in kilohertz (kHz) but the internal calculations use hertz (Hz) - convert here so downstream stages never have to re-convert
    out.operatingPoint.switchingFreqHz = units::kHzToHz(request.switchingFreqKHz);

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

    //Current-consistency check (spec: peak/RMS/ripple must describe one physically real waveform). Only
    //possible when ripple is supplied - that is the only case a minimum instantaneous inductor current can
    //be derived from. A negative minimum current means the supplied peak/ripple pair implies discontinuous
    //conduction mode, which Phase 1 does not model (mirrors BuckElectricalSolver's identical DCM rejection
    //for Mode 1 - same physics, same epsilon convention, applied here for the direct-entry Mode 2 path).
    if (out.operatingPoint.rippleCurrentPeakToPeakA.has_value()) {
        constexpr double kCcmZeroEpsilonA = 1e-6;
        double ripple = *out.operatingPoint.rippleCurrentPeakToPeakA;
        double minInductorCurrentA = out.operatingPoint.peakCurrentA - ripple;
        out.operatingPoint.minInductorCurrentA = minInductorCurrentA;
        out.operatingPoint.currentConsistencyStatus = EvaluationStatus::Evaluated;

        if (minInductorCurrentA < -kCcmZeroEpsilonA) {
            throw std::invalid_argument(
                "computed minimum inductor current is negative (" + std::to_string(minInductorCurrentA) +
                " A) given peakCurrentA=" + std::to_string(out.operatingPoint.peakCurrentA) +
                " A and rippleCurrentPeakToPeakA=" + std::to_string(ripple) +
                " A - this operating point enters discontinuous conduction mode (DCM), which Phase 1 does "
                "not support (CCM only)");
        } else if (minInductorCurrentA <= kCcmZeroEpsilonA) {
            out.operatingPoint.conductionMode = ConductionMode::CCMBoundary;
            out.operatingPoint.currentConsistencyExplanation =
                "minimum inductor current is at or near zero - this design sits at the CCM/DCM boundary; "
                "small load or line variation may push it into DCM";
        } else {
            out.operatingPoint.conductionMode = ConductionMode::CCM;
            out.operatingPoint.currentConsistencyExplanation =
                "peak (" + std::to_string(out.operatingPoint.peakCurrentA) + " A), ripple (" + std::to_string(ripple) +
                " A pk-pk), and minimum (" + std::to_string(minInductorCurrentA) +
                " A) inductor current are mutually consistent under the triangular-ripple CCM assumption";
        }

        //rmsCurrentA is bounded by [minInductorCurrentA, peakCurrentA] for any real current waveform whose
        //instantaneous value never leaves that range - a real physical bound, not a fabricated one. Only
        //checked when rmsCurrentA was supplied directly; a value this function itself derived from the same
        //ripple/average pair cannot contradict it.
        if (!out.operatingPoint.rmsCurrentDerived && minInductorCurrentA > -kCcmZeroEpsilonA) {
            double rms = out.operatingPoint.rmsCurrentA;
            if (rms > out.operatingPoint.peakCurrentA + kCcmZeroEpsilonA || rms < minInductorCurrentA - kCcmZeroEpsilonA) {
                throw std::invalid_argument(
                    "supplied rmsCurrentA (" + std::to_string(rms) +
                    " A) is physically inconsistent with peakCurrentA/rippleCurrentPeakToPeakA - RMS current "
                    "of a real waveform can never fall outside [minInductorCurrentA, peakCurrentA] = [" +
                    std::to_string(minInductorCurrentA) + ", " + std::to_string(out.operatingPoint.peakCurrentA) + "] A");
            }
        }
    }

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
