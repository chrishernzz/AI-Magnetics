#include "core/losses/LossEvaluation.h"
#include "core/losses/CopperLoss.h"
#include "core/losses/CoreLoss.h"

//precondition: see header
//postcondition: see header
LossEvaluationResult evaluateLosses(const MaterialCandidate& material, const CoreCandidate& core, const TurnsAndGapResult& turnsAndGap,
                                     const WindingDesignResult& winding, double rmsCurrentA, double switchingFreqHz,
                                     const std::optional<double>& rippleCurrentPeakToPeakA) {
    LossEvaluationResult result;

    if (winding.resistanceStatus == EvaluationStatus::Evaluated) {
        result.copperLossStatus = EvaluationStatus::Evaluated;
        result.copperLossW = calculateCopperLoss(rmsCurrentA, winding.dcrOhms);
    } else {
        result.copperLossStatus = EvaluationStatus::NotEvaluated;
        result.missingData.push_back("DC copper loss not evaluated: " +
                                      (winding.missingData.empty() ? std::string("DCR unavailable")
                                                                    : winding.missingData.front()));
    }

    // Core loss (Stage B): needs a real coefficient row for this
    // material/frequency AND a real ripple current to compute
    // flux-density swing from (Option 1 - never approximate Bswing from
    // peak flux, which would silently misrepresent a DC-biased inductor
    // as swinging symmetrically around zero).
    if (!rippleCurrentPeakToPeakA.has_value()) {
        result.coreLossStatus = EvaluationStatus::NotEvaluated;
        result.missingData.push_back(
            "core loss not evaluated: no rippleCurrentPeakToPeakA supplied - flux-density swing can only be "
            "computed from real ripple current, never approximated from peak flux");
    } else if (turnsAndGap.turns <= 0) {
        result.coreLossStatus = EvaluationStatus::NotEvaluated;
        result.missingData.push_back("core loss not evaluated: turns/gap design did not converge");
    } else {
        CoreLossCoefficientLookup lookup = findCoreLossCoefficients(material.materialFamily, switchingFreqHz);
        if (!lookup.found) {
            result.coreLossStatus = EvaluationStatus::NotEvaluated;
            result.missingData.push_back("core loss not evaluated: no validated Steinmetz coefficients for material '" +
                                          material.materialFamily + "' at this frequency");
        } else {
            double calculatedInductanceH = turnsAndGap.calculatedInductanceUH * 1e-6;
            double aeM2 = core.aeMm2 * 1e-6;
            double fluxDensitySwingT =
                (calculatedInductanceH * (*rippleCurrentPeakToPeakA)) / (static_cast<double>(turnsAndGap.turns) * aeM2);

            // PyOpenMagnetics/MAS sources these coefficients from a field literally named
            // "volumetricLosses" - SI convention, so Pv comes out in W/m^3, not W/cm^3 (confirmed
            // empirically: treating it as W/cm^3 produced ~300 W/cm^3 for a 20 mT swing, which is
            // physically absurd - W/m^3 gives a normal few-tens-of-milliwatt result instead).
            double coreLossDensityWPerM3 = calculateCoreLossDensity(lookup.coefficients, fluxDensitySwingT, switchingFreqHz);
            // Effective core volume: Ae (mm^2) * Le (mm) = mm^3, converted to m^3 (mm^3 * 1e-9).
            double coreVolumeM3 = (core.aeMm2 * core.leMm) * 1e-9;

            result.coreLossStatus = EvaluationStatus::Evaluated;
            result.coreLossW = coreLossDensityWPerM3 * coreVolumeM3;
        }
    }

    // Skin/proximity (high-frequency) loss: not implemented in Phase 1.
    result.highFrequencyLossStatus = EvaluationStatus::NotEvaluated;
    result.missingData.push_back(
        "high-frequency (skin/proximity) loss model is not implemented in Phase 1");

    return result;
}
