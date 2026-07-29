#include "backend/services/OperatingPointConfidenceService.h"

//precondition: see header
//postcondition: see header
OperatingPointConfidence classifyOperatingPointConfidence(const OperatingPoint& operatingPoint) {
    OperatingPointConfidence result;

    //Classifies what the request ORIGINALLY supplied, not what peakCurrentA holds now - a successful
    //Mode 2 derivation must never be misreported as a directly-supplied peak.
    bool peakDirectlySupplied = operatingPoint.peakCurrentA.has_value() && !operatingPoint.peakCurrentDerived;
    bool rippleSupplied = operatingPoint.rippleCurrentPeakToPeakA.has_value();

    if (peakDirectlySupplied && rippleSupplied) {
        result.inputMode = OperatingPointInputMode::FullOperatingPoint;
    } else if (peakDirectlySupplied) {
        result.inputMode = OperatingPointInputMode::RmsPlusPeak;
    } else if (rippleSupplied) {
        result.inputMode = OperatingPointInputMode::RmsPlusRipple;
    } else {
        result.inputMode = OperatingPointInputMode::RmsOnly;
    }

    if (!operatingPoint.peakCurrentA.has_value()) {
        result.peakCurrentProvenance = PeakCurrentProvenance::NotSupplied;
    } else if (operatingPoint.peakCurrentDerived) {
        result.peakCurrentProvenance = PeakCurrentProvenance::Derived;
    } else {
        result.peakCurrentProvenance = PeakCurrentProvenance::DirectlySupplied;
    }

    switch (result.inputMode) {
        case OperatingPointInputMode::RmsOnly:
            result.summary =
                "only inductance, RMS current, and switching frequency supplied - peak flux density and "
                "saturation margin cannot be evaluated (no peak current to compute them from)";
            break;
        case OperatingPointInputMode::RmsPlusRipple:
            if (result.peakCurrentProvenance == PeakCurrentProvenance::Derived) {
                result.summary =
                    "RMS + ripple supplied; peak current derived from RMS and ripple (triangular-ripple "
                    "assumption), not directly measured - saturation validation runs against the derived "
                    "value";
            } else {
                result.summary =
                    "RMS + ripple supplied, but a peak current could not be derived from them (the "
                    "RMS/ripple combination is not physically consistent with a triangular ripple "
                    "waveform) - peak flux density and saturation margin cannot be evaluated";
            }
            break;
        case OperatingPointInputMode::RmsPlusPeak:
            result.summary =
                "RMS + peak current directly supplied - saturation validation runs against a directly "
                "measured peak current";
            break;
        case OperatingPointInputMode::FullOperatingPoint:
            result.summary =
                "full operating point supplied (RMS, peak, and ripple current all directly measured) - "
                "the highest-confidence input available to this engine";
            break;
    }

    return result;
}
