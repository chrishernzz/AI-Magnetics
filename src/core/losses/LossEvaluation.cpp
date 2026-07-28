#include "core/losses/LossEvaluation.h"
#include "core/losses/CopperLoss.h"
#include "core/losses/CoreLoss.h"
#include "core/units/UnitConversions.h"

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
            double calculatedInductanceH = units::uHToH(turnsAndGap.calculatedInductanceUH);
            double aeM2 = units::mm2ToM2(core.aeMm2);
            double fluxDensitySwingT =
                (calculatedInductanceH * (*rippleCurrentPeakToPeakA)) / (static_cast<double>(turnsAndGap.turns) * aeM2);

            if (!fluxSwingWithinValidatedRange(lookup.coefficients, fluxDensitySwingT)) {
                result.coreLossStatus = EvaluationStatus::NotEvaluated;
                result.missingData.push_back(
                    "core loss not evaluated: flux-density swing " + std::to_string(fluxDensitySwingT) +
                    " T falls outside the coefficient row's validated range for material '" + material.materialFamily +
                    "' - never extrapolated silently");
                return result;
            }

            // The CSV's own minFluxSwingT/maxFluxSwingT columns don't exist yet (see the guard just
            // above, currently always a no-op), but a real per-material bound already does: the
            // Steinmetz power law (Pv = k*f^alpha*B^beta) is only a valid small-signal fit below the
            // material's own saturation flux density - past that point the core's real B-H curve goes
            // nonlinear and the "loss density" this formula would report explodes into physically
            // meaningless territory (confirmed empirically: candidates with fluxDensitySwingT computed
            // at 1-2+ T, in materials whose real BmaxT is ~0.4-0.5T, were reporting tens of thousands
            // of watts of "core loss" for a small inductor). Gate on the same real BmaxT that
            // SaturationValidation/PeakFluxValidation already use - never a new fabricated bound.
            if (material.hasBmaxData && fluxDensitySwingT > material.bmaxT) {
                result.coreLossStatus = EvaluationStatus::NotEvaluated;
                result.missingData.push_back(
                    "core loss not evaluated: flux-density swing " + std::to_string(fluxDensitySwingT) +
                    " T exceeds material '" + material.materialFamily + "'s real saturation flux density (" +
                    std::to_string(material.bmaxT) +
                    " T) - the Steinmetz power law is not a valid extrapolation past saturation, so no core-loss "
                    "watts figure is reported rather than showing a physically meaningless one");
                return result;
            }

            // PyOpenMagnetics/MAS sources these coefficients from a field literally named
            // "volumetricLosses" - SI convention, so Pv comes out in W/m^3, not W/cm^3 (confirmed
            // empirically: treating it as W/cm^3 produced ~300 W/cm^3 for a 20 mT swing, which is
            // physically absurd - W/m^3 gives a normal few-tens-of-milliwatt result instead).
            double coreLossDensityWPerM3 = calculateCoreLossDensity(lookup.coefficients, fluxDensitySwingT, switchingFreqHz);
            // Effective core volume: Ae (mm^2) * Le (mm) = mm^3, converted to m^3.
            double coreVolumeM3 = units::mm3ToM3(core.aeMm2 * core.leMm);

            result.coreLossStatus = EvaluationStatus::Evaluated;
            result.coreLossW = coreLossDensityWPerM3 * coreVolumeM3;

            result.coreLossMaterialUsed = material.materialFamily;
            result.coreLossCoefficientMinFreqHz = lookup.coefficients.minFreqHz;
            result.coreLossCoefficientMaxFreqHz = lookup.coefficients.maxFreqHz;
            result.coreLossFluxDensitySwingT = fluxDensitySwingT;
            result.coreLossVolumeM3 = coreVolumeM3;
            result.coreLossDensityWPerM3 = coreLossDensityWPerM3;
        }
    }

    return result;
}
