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
    //no conversion needed here because the request supplies peak current in amps and the internal calculations use amps (will be used later for checks such as peak flux density, core saturation, and energy storage) - optional, carried through as-is (see OperatingPoint::peakCurrentA for what's affected when it's absent)
    out.operatingPoint.peakCurrentA = request.peakCurrentA;
    //the request supplies switching frequency in kilohertz (kHz) but the internal calculations use hertz (Hz) - convert here so downstream stages never have to re-convert
    out.operatingPoint.switchingFreqHz = units::kHzToHz(request.switchingFreqKHz);

    //if true meaning provided the RMS value then go in here (not nil)
    if (request.rmsCurrentA.has_value()) {
        //deference to get the actual value : peak current and RMS current are never interchangeable (spec section 5) - an explicitly supplied RMS current is always used as-is, never inferred from peak current.
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

    //Current-consistency check (spec: peak/RMS/ripple must describe one physically real waveform). Only possible when BOTH peak and ripple are supplied - those are the only two values a minimum
    //instantaneous inductor current can be derived from. A genuine contradiction (implied DCM, or an out-of-envelope rmsCurrentA) never throws and never blocks generation of a design - see
    //OperatingPoint::currentConsistencyStatus. It sets NotEvaluated with a clear explanation instead, exactly like any other missing-data case; everything that doesn't depend on this (saturation via
    //peak alone, fill/current-density via RMS, core loss from the literal ripple value) still runs.
    constexpr double kCcmZeroEpsilonA = 1e-6;

    //Mode 2 peak derivation: peak current is absent but rmsCurrentA (however it was obtained above) and rippleCurrentPeakToPeakA are both known. Irms^2 = Iavg^2 + ripple^2/12 (the same triangular-ripple
    //formula already trusted above for the average+ripple -> RMS direction) can be run backwards: Iavg = sqrt(Irms^2 - ripple^2/12), peak = Iavg + ripple/2 - a real algebraic derivation, not a new
    //assumption, and provably never less than rmsCurrentA (Irms^2 = Iavg^2 + ripple^2/12 <= (Iavg + ripple/2)^2 = peak^2 for any Iavg >= 0), so this can never trip the rms > peak sanity check below.
    //Only attempted when Irms^2 >= ripple^2/12 - otherwise the supplied RMS/ripple combination is itself inconsistent with a triangular waveform, and no exception is thrown (matches this file's
    //soften-don't-crash policy for ripple-dependent checks) - peakCurrentA simply stays absent.
    if (!out.operatingPoint.peakCurrentA.has_value() && out.operatingPoint.rippleCurrentPeakToPeakA.has_value()) {
        double irms = out.operatingPoint.rmsCurrentA;
        double ripple = *out.operatingPoint.rippleCurrentPeakToPeakA;
        double rippleTermSquared = (ripple * ripple) / 12.0;

        if (irms * irms >= rippleTermSquared) {
            double iavgDerived = std::sqrt(irms * irms - rippleTermSquared);
            double peakDerived = iavgDerived + ripple / 2.0;
            out.operatingPoint.peakCurrentA = peakDerived;
            out.operatingPoint.peakCurrentDerived = true;
            out.operatingPoint.peakCurrentAssumption =
                "peakCurrentA derived from rmsCurrentA and rippleCurrentPeakToPeakA assuming a triangular "
                "ripple waveform (Iavg = sqrt(Irms^2 - ripple^2/12), peak = Iavg + ripple/2) - derived from "
                "the supplied RMS and ripple values, not an independent cross-check against them, and not a "
                "directly measured value. Supply peakCurrentA directly if the real waveform is not triangular.";
        } 
        else {
            out.operatingPoint.currentConsistencyExplanation =
                "no peakCurrentA supplied, and it cannot be derived from rmsCurrentA (" + std::to_string(irms) +
                " A) and rippleCurrentPeakToPeakA (" + std::to_string(ripple) +
                " A pk-pk) - Irms^2 (" + std::to_string(irms * irms) + ") is below ripple^2/12 (" +
                std::to_string(rippleTermSquared) +
                "), which is physically impossible for a triangular ripple waveform with this RMS/ripple "
                "combination; core loss is still computed from the ripple value on its own";
        }
    }

    if (out.operatingPoint.peakCurrentA.has_value() && out.operatingPoint.rippleCurrentPeakToPeakA.has_value()) {
        double peak = *out.operatingPoint.peakCurrentA;
        double ripple = *out.operatingPoint.rippleCurrentPeakToPeakA;
        double minInductorCurrentA = peak - ripple;
        out.operatingPoint.minInductorCurrentA = minInductorCurrentA;

        //true or false and states if it is DCM mode
        bool dcmImplied = minInductorCurrentA < -kCcmZeroEpsilonA;
        bool rmsOutOfEnvelope = false;
        if (!dcmImplied && !out.operatingPoint.rmsCurrentDerived) {
            double rms = out.operatingPoint.rmsCurrentA;
            rmsOutOfEnvelope = rms > peak + kCcmZeroEpsilonA || rms < minInductorCurrentA - kCcmZeroEpsilonA;
        }

        if (dcmImplied) {
            out.operatingPoint.currentConsistencyStatus = EvaluationStatus::NotEvaluated;
            out.operatingPoint.conductionMode = ConductionMode::DCMUnsupported;
            out.operatingPoint.currentConsistencyExplanation =
                "computed minimum inductor current is negative (" + std::to_string(minInductorCurrentA) +
                " A) given peak=" + std::to_string(peak) + " A" +
                (out.operatingPoint.peakCurrentDerived ? " (derived from rmsCurrentA and ripple)" : "") +
                " and ripple=" + std::to_string(ripple) +
                " A pk-pk - this combination implies discontinuous conduction mode (DCM), which Phase 1 "
                "does not model (CCM only); conduction mode and core loss from this ripple value are still "
                "reported, but the consistency check itself is not evaluated";
        } 
        else if (rmsOutOfEnvelope) {
            out.operatingPoint.currentConsistencyStatus = EvaluationStatus::NotEvaluated;
            out.operatingPoint.conductionMode = minInductorCurrentA <= kCcmZeroEpsilonA ? ConductionMode::CCMBoundary : ConductionMode::CCM;
            out.operatingPoint.currentConsistencyExplanation =
                "supplied rmsCurrentA (" + std::to_string(out.operatingPoint.rmsCurrentA) +
                " A) is physically inconsistent with peakCurrentA/rippleCurrentPeakToPeakA - RMS current of "
                "a real waveform can never fall outside [minInductorCurrentA, peakCurrentA] = [" +
                std::to_string(minInductorCurrentA) + ", " + std::to_string(peak) +
                "] A; peak/ripple-derived conduction mode is still reported, but this check is not evaluated";
        } 
        else if (minInductorCurrentA <= kCcmZeroEpsilonA) {
            out.operatingPoint.currentConsistencyStatus = EvaluationStatus::Evaluated;
            out.operatingPoint.conductionMode = ConductionMode::CCMBoundary;
            out.operatingPoint.currentConsistencyExplanation =
                "minimum inductor current is at or near zero - this design sits at the CCM/DCM boundary; "
                "small load or line variation may push it into DCM";
        }
        else if (out.operatingPoint.peakCurrentDerived) {
            //peak was derived FROM this same rms/ripple pair above, so this branch is confirming the derivation's own internal self-consistency (it is provably always self-consistent - see the
            //derivation comment above), not an independent cross-check against a separately-measured peak.
            out.operatingPoint.currentConsistencyStatus = EvaluationStatus::Evaluated;
            out.operatingPoint.conductionMode = ConductionMode::CCM;
            out.operatingPoint.currentConsistencyExplanation =
                "peak current (" + std::to_string(peak) + " A) was derived from rmsCurrentA and ripple (" +
                std::to_string(ripple) + " A pk-pk), not independently measured - minimum inductor current (" +
                std::to_string(minInductorCurrentA) + " A) reflects that same derivation, under the "
                "triangular-ripple CCM assumption";
        } 
        else {
            out.operatingPoint.currentConsistencyStatus = EvaluationStatus::Evaluated;
            out.operatingPoint.conductionMode = ConductionMode::CCM;
            out.operatingPoint.currentConsistencyExplanation =
                "peak (" + std::to_string(peak) + " A), ripple (" + std::to_string(ripple) +
                " A pk-pk), and minimum (" + std::to_string(minInductorCurrentA) +
                " A) inductor current are mutually consistent under the triangular-ripple CCM assumption";
        }
    } 
    else if (!out.operatingPoint.peakCurrentA.has_value() && out.operatingPoint.rippleCurrentPeakToPeakA.has_value()) {
        out.operatingPoint.currentConsistencyExplanation =
            "no peakCurrentA supplied - minimum inductor current and conduction mode cannot be derived from ripple alone (core loss is still computed from the ripple value on its own)";
    }
    else if (out.operatingPoint.peakCurrentA.has_value() && !out.operatingPoint.rippleCurrentPeakToPeakA.has_value()) {
        out.operatingPoint.currentConsistencyExplanation =
            "no rippleCurrentPeakToPeakA supplied - minimum inductor current and conduction mode cannot be derived from peak current alone";
    } 
    else {
        out.operatingPoint.currentConsistencyExplanation =
            "neither peakCurrentA nor rippleCurrentPeakToPeakA supplied";
    }

    //Basic RMS-vs-peak sanity check, independent of ripple: RMS of any real waveform can never exceed its own peak. Kept as a hard rejection (not softened like the ripple-dependent checks above) since it's
    //an unambiguous contradiction between two directly-entered numbers, not an assumption-dependent one.
    if (out.operatingPoint.peakCurrentA.has_value() && !out.operatingPoint.rmsCurrentDerived) {
        double peak = *out.operatingPoint.peakCurrentA;
        double rms = out.operatingPoint.rmsCurrentA;
        if (rms > peak + 1e-6) {
            throw std::invalid_argument("supplied rmsCurrentA (" + std::to_string(rms) + " A) cannot exceed peakCurrentA (" + std::to_string(peak) + " A) - a real waveform's RMS value never exceeds its own peak");
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
