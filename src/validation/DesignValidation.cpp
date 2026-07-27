#include "DesignValidation.h"
#include "core/units/UnitConversions.h"
#include <cmath>

namespace {

//Formula for Peak Flux Density: Bpk(T) = L(H) * Ipk(A) / (N * Ae(m^2))
double calculatePeakFluxDensityT(const CoreCandidate& core, const TurnsAndGapResult& turnsAndGap, double peakCurrentA) {
    //base case making sure turns is a real value
    if (turnsAndGap.turns <= 0) {
        return 0.0;
    }
    double inductanceH = units::uHToH(turnsAndGap.calculatedInductanceUH);
    double aeM2 = units::mm2ToM2(core.aeMm2);
    return (inductanceH * peakCurrentA) / (static_cast<double>(turnsAndGap.turns) * aeM2);
}

struct FluxLimit {
    double limitT;
    bool usedDefault;
};

FluxLimit applicableFluxLimit(const MaterialCandidate& material, const DesignRules& rules) {
    if (material.hasBmaxData) {
        return {material.bmaxT, false};
    }
    return {rules.defaultFluxDensityLimitT, true};
}

}  // namespace

//precondition: none
//postcondition: passes only if turns/gap converged and the resulting inductance is within tolerance
ValidationResult InductanceValidation(const TurnsAndGapResult& turnsAndGap, double tolerancePercent) {
    ValidationResult result;
    result.checkName = "InductanceValidation";
    result.unit = "%";
    result.limitValue = tolerancePercent;

    if (!turnsAndGap.converged) {
        result.passed = false;
        result.calculatedValue = 0.0;
        result.explanation = "turns/gap design did not converge - no calculated inductance to check";
        return result;
    }

    result.calculatedValue = std::abs(turnsAndGap.inductanceErrorPercent);
    result.passed = turnsAndGap.withinTolerance;
    result.explanation = "calculated inductance " + std::to_string(turnsAndGap.calculatedInductanceUH) + " uH vs target (error " + std::to_string(turnsAndGap.inductanceErrorPercent) + "%), tolerance " + std::to_string(tolerancePercent) + "%";
    return result;
}

//precondition: turnsAndGap.converged
//postcondition: passes if calculated peak flux density is at or below the applicable limit (material-specific if available, else the Phase 1 default)
ValidationResult PeakFluxValidation(const CoreCandidate& core, const MaterialCandidate& material, const TurnsAndGapResult& turnsAndGap, double peakCurrentA, const DesignRules& rules) {
    ValidationResult result;
    result.checkName = "PeakFluxValidation";
    result.unit = "T";

    //call the function from namespace to get the Peak Flux Density value
    double bpk = calculatePeakFluxDensityT(core, turnsAndGap, peakCurrentA);
    FluxLimit limit = applicableFluxLimit(material, rules);

    result.calculatedValue = bpk;
    result.limitValue = limit.limitT;
    result.usedDefaultLimit = limit.usedDefault;
    result.passed = turnsAndGap.converged && bpk <= limit.limitT;
    result.explanation = "peak flux density " + std::to_string(bpk) + " T vs limit " +
                          std::to_string(limit.limitT) + " T (" +
                          (limit.usedDefault ? std::string("Phase 1 default - material '") + material.materialFamily +
                                                   "' has no measured BmaxT"
                                             : std::string("material-specific value for '") +
                                                   material.materialFamily + "'") +
                          ")";
    return result;
}

//precondition: turnsAndGap.converged
//postcondition: passes if the margin between the applicable limit and the calculated peak flux density meets rules.minimumSaturationMarginPercent
ValidationResult SaturationValidation(const CoreCandidate& core, const MaterialCandidate& material, const TurnsAndGapResult& turnsAndGap, double peakCurrentA, const DesignRules& rules) {
    ValidationResult result;
    result.checkName = "SaturationValidation";
    result.unit = "%";
    result.limitValue = rules.minimumSaturationMarginPercent;

    double bpk = calculatePeakFluxDensityT(core, turnsAndGap, peakCurrentA);
    FluxLimit limit = applicableFluxLimit(material, rules);

    double marginPercent = limit.limitT > 0.0 ? 100.0 * (limit.limitT - bpk) / limit.limitT : 0.0;

    result.calculatedValue = marginPercent;
    result.usedDefaultLimit = limit.usedDefault;
    result.passed = turnsAndGap.converged && marginPercent >= rules.minimumSaturationMarginPercent;
    result.explanation = "saturation margin " + std::to_string(marginPercent) + "% vs required " + std::to_string(rules.minimumSaturationMarginPercent) + "% (" +
                          (limit.usedDefault ? "against the Phase 1 default flux limit, not a material fact"
                                             : "against material-specific BmaxT") +
                          ")";
    return result;
}

//precondition: none
//postcondition: passes if the realistic physical window fill (insulated conductors, packing factor,
//bobbin/margin/lead-exit derates - see WindingDesign.h) is at or below the maximum. Gates on
//physicalWindowFillFactor, not the raw copper-only fillFactor - an intentional behavior change from the
//Phase 1 stub: a candidate that passed on raw copper fill alone can now fail here.
ValidationResult WindingFitValidation(const WindingDesignResult& winding, const DesignRules& rules) {
    ValidationResult result;
    result.checkName = "WindingFitValidation";
    result.unit = "fraction";
    result.calculatedValue = winding.physicalWindowFillFactor;
    result.limitValue = rules.maximumFillFactor;
    result.passed = winding.fitsPhysicalWindow;
    result.explanation = "physical window fill " + std::to_string(winding.physicalWindowFillFactor) + " vs maximum " +
                          std::to_string(rules.maximumFillFactor) + " (raw copper-only fill was " +
                          std::to_string(winding.fillFactor) + ")";
    return result;
}

//precondition: none
//postcondition: passes if the winding's actual current density (afterbrounding to a real AWG gauge) is at or below the allowable density
ValidationResult CurrentDensityValidation(const WindingDesignResult& winding, const DesignRules& rules) {
    ValidationResult result;
    result.checkName = "CurrentDensityValidation";
    result.unit = "A/mm^2";

    //A/cm^2 -> A/mm^2
    double allowableAPerMm2 = units::aPerCm2ToAPerMm2(rules.allowableCurrentDensityAperCm2);

    result.calculatedValue = winding.currentDensityAperMm2;
    result.limitValue = allowableAPerMm2;
    result.passed = winding.currentDensityAperMm2 <= allowableAPerMm2;
    result.explanation = "actual current density " + std::to_string(winding.currentDensityAperMm2) + " A/mm^2 vs allowable " + std::to_string(allowableAPerMm2) + " A/mm^2";
    return result;
}

//precondition: none
//postcondition: passes only when thermal.status == PreliminaryThermalEstimate and the predicted rise is within allowableTempRiseC;
//otherwise not_evaluated (passed=false, never an assumed pass - spec section 10). ThermalStatus has no "fully evaluated" value
//(see ThermalEvaluation.h), so a passing result here always carries isPreliminaryEstimate=true - the numeric check genuinely
//ran and passed/failed, but rests on a Phase 1 coarse thermal-resistance constant, never per-core measured/simulated data.
ValidationResult ThermalValidation(const ThermalEvaluationResult& thermal, double allowableTempRiseC) {
    ValidationResult result;
    result.checkName = "ThermalValidation";
    result.unit = "C";
    result.limitValue = allowableTempRiseC;

    if (thermal.status != ThermalStatus::PreliminaryThermalEstimate) {
        result.calculatedValue = 0.0;
        result.passed = false;
        result.status = EvaluationStatus::NotEvaluated;
        result.explanation = "not evaluated: " + thermal.missingDataExplanation;
        return result;
    }

    result.calculatedValue = thermal.predictedTempRiseC;
    result.passed = thermal.predictedTempRiseC <= allowableTempRiseC;
    result.isPreliminaryEstimate = true;
    result.explanation = "predicted temperature rise " + std::to_string(thermal.predictedTempRiseC) + " C vs allowable " +
                          std::to_string(allowableTempRiseC) + " C (Rth=" +
                          std::to_string(thermal.thermalResistanceCPerWUsed) + " C/W is a Phase 1 default, not per-core measured data)";
    return result;
}

//precondition: none
//postcondition: see header
FluxLimitTiers calculateFluxLimitTiers(const MaterialCandidate& material, const DesignRules& rules) {
    FluxLimitTiers tiers;
    FluxLimit limit = applicableFluxLimit(material, rules);
    tiers.absoluteSaturationT = limit.limitT;
    tiers.absoluteSaturationIsDefault = limit.usedDefault;
    tiers.recommendedOperatingT = limit.limitT * rules.recommendedFluxDerateFactor;
    return tiers;
}
