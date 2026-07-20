#include "DesignValidation.h"
#include <cmath>

namespace {

//Formula for Peak Flux Density: Bpk(T) = L(H) * Ipk(A) / (N * Ae(m^2))
double calculatePeakFluxDensityT(const CoreCandidate& core, const TurnsAndGapResult& turnsAndGap, double peakCurrentA) {
    //base case making sure turns is a real value
    if (turnsAndGap.turns <= 0) {
        return 0.0;
    }
    double inductanceH = turnsAndGap.calculatedInductanceUH * 1e-6;
    double aeM2 = core.aeMm2 * 1e-6;
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
//postcondition: passes if fill factor is at or below the maximum
ValidationResult WindingFitValidation(const WindingDesignResult& winding, const DesignRules& rules) {
    ValidationResult result;
    result.checkName = "WindingFitValidation";
    result.unit = "fraction";
    result.calculatedValue = winding.fillFactor;
    result.limitValue = rules.maximumFillFactor;
    result.passed = winding.fitsWindow;
    result.explanation = "fill factor " + std::to_string(winding.fillFactor) + " vs maximum " + std::to_string(rules.maximumFillFactor);
    return result;
}

//precondition: none
//postcondition: passes if the winding's actual current density (afterbrounding to a real AWG gauge) is at or below the allowable density
ValidationResult CurrentDensityValidation(const WindingDesignResult& winding, const DesignRules& rules) {
    ValidationResult result;
    result.checkName = "CurrentDensityValidation";
    result.unit = "A/mm^2";

    //A/cm^2 -> A/mm^2
    double allowableAPerMm2 = rules.allowableCurrentDensityAperCm2 / 100.0;  

    result.calculatedValue = winding.currentDensityAperMm2;
    result.limitValue = allowableAPerMm2;
    result.passed = winding.currentDensityAperMm2 <= allowableAPerMm2;
    result.explanation = "actual current density " + std::to_string(winding.currentDensityAperMm2) + " A/mm^2 vs allowable " + std::to_string(allowableAPerMm2) + " A/mm^2";
    return result;
}

//precondition: none
//postcondition: passes only when thermal.status == Evaluated and the predicted rise is within allowableTempRiseC; otherwise not_evaluated (passed=false, never an assumed pass - spec section 10)
ValidationResult ThermalValidation(const ThermalEvaluationResult& thermal, double allowableTempRiseC) {
    ValidationResult result;
    result.checkName = "ThermalValidation";
    result.unit = "C";
    result.limitValue = allowableTempRiseC;

    if (thermal.status != EvaluationStatus::Evaluated) {
        result.calculatedValue = 0.0;
        result.passed = false;
        result.status = EvaluationStatus::NotEvaluated;
        result.explanation = "not evaluated: " + thermal.missingDataExplanation;
        return result;
    }

    result.calculatedValue = thermal.predictedTempRiseC;
    result.passed = thermal.predictedTempRiseC <= allowableTempRiseC;
    result.explanation = "predicted temperature rise " + std::to_string(thermal.predictedTempRiseC) + " C vs allowable " + std::to_string(allowableTempRiseC) + " C";
    return result;
}
