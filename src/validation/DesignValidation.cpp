#include "DesignValidation.h"
#include "core/sizing/FluxLimit.h"
#include "core/units/UnitConversions.h"
#include <cmath>

namespace {

//Formula for Peak Flux Density: Bpk(T) = L(H) * Ipk(A) / (N * Ae(m^2))
//precondition: peakCurrentA is a real supplied value (never called when absent - see PeakFluxValidation/SaturationValidation)
//postcondition: returns the calculated peak flux density in Tesla, or 0.0 if turnsAndGap.turns <= 0 (no real turns to compute with)
double calculatePeakFluxDensityT(const CoreCandidate& core, const TurnsAndGapResult& turnsAndGap, double peakCurrentA) {
    //base case making sure turns is a real value
    if (turnsAndGap.turns <= 0) {
        return 0.0;
    }
    /*
    Uses loadedInductanceUH (the real inductance this fixed winding delivers AT peakCurrentA, after DC-bias roll-off), never calculatedInductanceUH (the zero-bias design value turns was solved to hit -
    see TurnsAndGapDesign.h). Saturation risk has to be judged against what the core actually does at real current, not what it does at 0A; for machined-gap/ferrite cores and any distributed-gap core with
    no rolloff data, loadedInductanceUH is set equal to calculatedInductanceUH, so this is a no-op there.
    */
    double inductanceH = units::uHToH(turnsAndGap.loadedInductanceUH);
    double aeM2 = units::mm2ToM2(core.aeMm2);
    //using the formual from above
    return (inductanceH * peakCurrentA) / (static_cast<double>(turnsAndGap.turns) * aeM2);
}

//precondition: none
//postcondition: returns the flux density at rmsCurrentA - used only as a guaranteed LOWER BOUND on the unsupplied real peak current (RMS <= peak always, for any unidirectional inductor-current waveform) - if,
//and only if, that floor already exceeds the applicable limit: a CERTAIN failure, since the real (unknown, >= RMS) peak can only make it worse. Returns nullopt when rmsCurrentA <= 0.0 (no current data to compute
//even a floor from) or when the floor doesn't prove anything - this never confirms safety, only failure.
std::optional<double> bpkAtCertainFailureRmsFloor(const CoreCandidate& core, const TurnsAndGapResult& turnsAndGap, double rmsCurrentA, const FluxLimit& limit) {
    if (rmsCurrentA <= 0.0) {
        return std::nullopt;
    }
    double bpkAtRmsFloor = calculatePeakFluxDensityT(core, turnsAndGap, rmsCurrentA);
    if (bpkAtRmsFloor > limit.limitT) {
        return bpkAtRmsFloor;
    }
    return std::nullopt;
}

//precondition: none
//postcondition: returns what mode it is in
std::string conductionModeName(ConductionMode mode) {
    switch (mode) {
        case ConductionMode::CCM:
            return "CCM";
        case ConductionMode::CCMBoundary:
            return "CCMBoundary";
        case ConductionMode::DCMUnsupported:
            return "DCMUnsupported";
    }
    return "unknown";
}

}  //namespace

//precondition: none
//postcondition: passes only if peakCurrentA/rmsCurrentA/rippleCurrentPeakToPeakA describe one physically consistent waveform (spec: current-consistency validation). NotEvaluated whenever peak or ripple is missing, OR when both are present but genuinely contradict each other (implied DCM, or an out-of-envelope rmsCurrentA) - 
//RequirementDerivationService::derive() never throws on this anymore, it always produces a real OperatingPoint with a clear currentConsistencyExplanation either way, so a design can still be generated from whatever data is actually usable.
ValidationResult CurrentConsistencyValidation(const OperatingPoint& operatingPoint) {
    ValidationResult result;
    //set the name, unit, and if it is mandatory
    result.checkName = "CurrentConsistencyValidation";
    result.unit = "A";
    result.mandatory = true;

    if (operatingPoint.currentConsistencyStatus != EvaluationStatus::Evaluated) {
        /*
        Covers three honest reasons, all reported via the same explanation RequirementDerivationService::derive() already composed: peak missing, ripple missing, or both
        present but genuinely contradictory (implied DCM / RMS outside the real envelope). Never a rejection reason - see InductorDesignService.cpp's aggregation, which only blocks a candidate on
        an Evaluated+failed check.
        */
        result.status = EvaluationStatus::NotEvaluated;
        result.passed = false;
        result.calculatedValue = operatingPoint.minInductorCurrentA;
        result.explanation = "not evaluated: " + operatingPoint.currentConsistencyExplanation;
        return result;
    }

    //Peak and ripple were both supplied and are mutually consistent with RMS - a real, clean result.
    result.calculatedValue = operatingPoint.minInductorCurrentA;
    result.limitValue = 0.0;
    result.passed = true;
    result.explanation = "conduction mode " + conductionModeName(operatingPoint.conductionMode) + ", minimum inductor current " + std::to_string(operatingPoint.minInductorCurrentA) + " A vs peak " + std::to_string(*operatingPoint.peakCurrentA) + " A (" + operatingPoint.currentConsistencyExplanation + ")";
    return result;
}

//precondition: none
//postcondition: passes only if turns/gap converged and the resulting inductance is within tolerance
ValidationResult InductanceValidation(const TurnsAndGapResult& turnsAndGap, double tolerancePercent) {
    ValidationResult result;
    //set the name, unit, and the limit tolerance
    result.checkName = "InductanceValidation";
    result.unit = "%";
    result.limitValue = tolerancePercent;

    if (!turnsAndGap.converged) {
        /*
        Non-convergence is a clean reject - TurnsAndGapDesign.cpp no longer reports a last-attempted turns/calculatedInductanceUH on this path (both stay at their struct defaults, 0), so this
        check doesn't report them either. Previously it did, on the reasoning that "how close did it get" was useful context - a real user report: that read as a near-real design to an engineer
        scanning results, not as the invalid intermediate point it was, even with FAIL/REJECT right next to it. This never turns a non-converged design into a pass (still unconditionally
        result.passed=false below) - it's honest about there being no real number to report at all.
        */
        result.status = EvaluationStatus::Evaluated;
        result.passed = false;
        result.calculatedValue = 0.0;
        result.explanation = "turns/gap design did not converge for this core/material combination at the requested operating current - no valid design exists to check inductance against; see TurnsAndGapDesign's own rejection reason for why";
        return result;
    }

    result.calculatedValue = std::abs(turnsAndGap.inductanceErrorPercent);
    result.passed = turnsAndGap.withinTolerance;
    result.explanation = "calculated inductance " + std::to_string(turnsAndGap.calculatedInductanceUH) + " uH vs target (error " + std::to_string(turnsAndGap.inductanceErrorPercent) + "%), tolerance " + std::to_string(tolerancePercent) + "%";
    return result;
}

//precondition: turnsAndGap.converged
//postcondition: passes if calculated peak flux density is at or below the applicable limit (material-specific if available, else the Phase 1 default). When peakCurrentA is absent, real peak is never substituted from RMS - but see bpkAtCertainFailureRmsFloor for the certain-failure lower-bound check that still runs.
ValidationResult PeakFluxValidation(const CoreCandidate& core, const MaterialCandidate& material, const TurnsAndGapResult& turnsAndGap, const std::optional<double>& peakCurrentA, double rmsCurrentA, const DesignRules& rules) {
    ValidationResult result;
    //set the name, unit, and if it is mandatory
    result.checkName = "PeakFluxValidation";
    result.unit = "T";
    result.mandatory = true;

    //if no value then we are going to use the rms current
    if (!peakCurrentA.has_value()) {
        FluxLimit limit = applicableFluxLimit(material, rules);
        result.limitValue = limit.limitT;
        result.usesDefaultAssumption = limit.usedDefault;
        if (auto bpkAtRmsFloor = bpkAtCertainFailureRmsFloor(core, turnsAndGap, rmsCurrentA, limit)) {
            result.status = EvaluationStatus::Evaluated;
            result.passed = false;
            result.isRmsLowerBoundRejection = true;
            result.calculatedValue = *bpkAtRmsFloor;
            result.explanation = "certain failure: peak flux density is already " + std::to_string(*bpkAtRmsFloor) + " T using RMS current (" + std::to_string(rmsCurrentA) + " A) alone as a guaranteed LOWER BOUND on the real (unsupplied) peak current, vs limit " + std::to_string(limit.limitT) + " T. Real peak current is always >= RMS, so this can only get worse - saturation is guaranteed regardless of ripple.";
            return result;
        }
        result.status = EvaluationStatus::NotEvaluated;
        result.passed = false;
        result.calculatedValue = 0.0;
        result.explanation = "not evaluated: no peakCurrentA supplied - peak flux density cannot be computed without it (never inferred from RMS current)";
        return result;
    }

    //call the function from namespace to get the Peak Flux Density value
    double bpk = calculatePeakFluxDensityT(core, turnsAndGap, *peakCurrentA);
    FluxLimit limit = applicableFluxLimit(material, rules);

    result.calculatedValue = bpk;
    result.limitValue = limit.limitT;
    result.usesDefaultAssumption = limit.usedDefault;
    result.passed = turnsAndGap.converged && bpk <= limit.limitT;
    /*
    a real user report: this check could show a calculatedValue well under limitValue (e.g. 0.002T vs an 0.8T limit) and still read FAIL, with no explanation of why. Root cause: bpk is computed from
    turnsAndGap.calculatedInductanceUH/turns, which on a non-converged design used to be the collapsed last-attempted point, not a real operating point - misleadingly small-looking numbers
    that still (correctly) failed via turnsAndGap.converged above. TurnsAndGapDesign.cpp no longer reports those last-attempted values at all (they stay at their struct defaults, 0 - see
    calculatePeakFluxDensityT's own turns<=0 guard above, which is why bpk is exactly 0.0 here on this path), so there's nothing left to misreport; the explanation says that plainly instead.
    */
    result.explanation = !turnsAndGap.converged? "turns/gap design did not converge for this core/material combination at the requested operating current - no valid design exists to check peak flux density against; see TurnsAndGapDesign's own rejection reason for why" : "peak flux density " + std::to_string(bpk) + " T vs limit " + std::to_string(limit.limitT) + " T (" + (limit.usedDefault ? std::string("Phase 1 default - material '") + material.materialFamily + "' has no measured BmaxT" : std::string("material-specific value for '") + material.materialFamily + "'") + ")";
    return result;
}

//precondition: turnsAndGap.converged
//postcondition: passes if the margin between the applicable limit and the calculated peak flux density meets rules.minimumSaturationMarginPercent. When peakCurrentA is absent, see bpkAtCertainFailureRmsFloor for the certain-failure lower-bound check that still runs.
ValidationResult SaturationValidation(const CoreCandidate& core, const MaterialCandidate& material, const TurnsAndGapResult& turnsAndGap, const std::optional<double>& peakCurrentA, double rmsCurrentA, const DesignRules& rules) {
    ValidationResult result;
    //set the name, unit, and if it is mandatory
    result.checkName = "SaturationValidation";
    result.unit = "%";
    result.limitValue = rules.minimumSaturationMarginPercent;
    result.mandatory = true;

    //if no peak current then you use rms current
    if (!peakCurrentA.has_value()) {
        FluxLimit limit = applicableFluxLimit(material, rules);
        result.usesDefaultAssumption = limit.usedDefault;
        if (auto bpkAtRmsFloor = bpkAtCertainFailureRmsFloor(core, turnsAndGap, rmsCurrentA, limit)) {
            double marginPercent = limit.limitT > 0.0 ? 100.0 * (limit.limitT - *bpkAtRmsFloor) / limit.limitT : 0.0;
            result.status = EvaluationStatus::Evaluated;
            result.passed = false;
            result.isRmsLowerBoundRejection = true;
            result.calculatedValue = marginPercent;
            result.explanation = "certain failure: saturation margin is already " + std::to_string(marginPercent) + "% (negative = already past the limit) using RMS current (" + std::to_string(rmsCurrentA) + " A) alone as a guaranteed LOWER BOUND on the real (unsupplied) peak current. Real peak current is always >= RMS, so this can only get worse - saturation is guaranteed regardless of ripple.";
            return result;
        }
        result.status = EvaluationStatus::NotEvaluated;
        result.passed = false;
        result.calculatedValue = 0.0;
        result.explanation = "not evaluated: no peakCurrentA supplied - saturation margin cannot be computed without it (never inferred from RMS current)";
        return result;
    }

    double bpk = calculatePeakFluxDensityT(core, turnsAndGap, *peakCurrentA);
    FluxLimit limit = applicableFluxLimit(material, rules);

    double marginPercent = limit.limitT > 0.0 ? 100.0 * (limit.limitT - bpk) / limit.limitT : 0.0;

    result.calculatedValue = marginPercent;
    result.usesDefaultAssumption = limit.usedDefault;
    result.passed = turnsAndGap.converged && marginPercent >= rules.minimumSaturationMarginPercent;
    /*
    same non-convergence case as PeakFluxValidation above - bpk is exactly 0.0 on a non-converged design (turns stays at its struct default 0, and calculatePeakFluxDensityT guards on turns<=0),
    which makes marginPercent look like a large, reassuring margin even though FAIL is correct (turnsAndGap.converged forces passed=false above). Say plainly that there's no real design to
    check at all, instead of reporting a margin number that doesn't mean what it looks like it means.
    */
    result.explanation = !turnsAndGap.converged ? "turns/gap design did not converge for this core/material combination at the requested operating current - no valid design exists to check saturation margin against; see TurnsAndGapDesign's own rejection reason for why" : "saturation margin " + std::to_string(marginPercent) + "% vs required " + std::to_string(rules.minimumSaturationMarginPercent) + "% (" + (limit.usedDefault ? "against the Phase 1 default flux limit, not a material fact" : "against material-specific BmaxT") + ")";
    return result;
}

//precondition: none
//postcondition: passes if the realistic physical window fill (insulated conductors, packing factor, bobbin/margin/lead-exit derates - see WindingDesign.h) is at or below the maximum. Gates on
//physicalWindowFillFactor, not the raw copper-only fillFactor - an intentional behavior change from the Phase 1 stub: a candidate that passed on raw copper fill alone can now fail here.
ValidationResult WindingFitValidation(const WindingDesignResult& winding, const DesignRules& rules, bool turnsConverged) {
    ValidationResult result;
    result.checkName = "WindingFitValidation";
    result.unit = "fraction";
    /*
    a real user report: on a non-converged design, designWinding() is still called downstream with turns=0 (see InductorDesignService.cpp - kept for the other, genuinely turns-independent checks
    this same call feeds, see CurrentDensityValidation below), which makes physicalWindowFillFactor trivially 0.0 - a wire count of zero always "fits," which reads as a real PASS for a design that
    doesn't exist. Report NotEvaluated instead of a trivially-true pass.
    */
    if (!turnsConverged) {
        result.status = EvaluationStatus::NotEvaluated;
        result.passed = false;
        result.calculatedValue = 0.0;
        result.explanation = "not evaluated: turns/gap design did not converge - there is no real winding to check physical fit for";
        return result;
    }
    result.calculatedValue = winding.physicalWindowFillFactor;
    result.limitValue = rules.maximumFillFactor;
    result.passed = winding.fitsPhysicalWindow;
    result.explanation = "physical window fill " + std::to_string(winding.physicalWindowFillFactor) + " vs maximum " + std::to_string(rules.maximumFillFactor) + " (raw copper-only fill was " + std::to_string(winding.fillFactor) + ")";
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
//postcondition: passes if a parallel-strand bundle (laid side by side, insulatedConductorDiameterMm each) fits within the core's real narrowest window opening. NotEvaluated for a single-strand winding, or any
//core with no real linear window dimension (every toroid, or a two-piece core missing the data) - see WindingDesign.cpp for exactly when bundleFitStatus becomes Evaluated.
ValidationResult BundleFitValidation(const WindingDesignResult& winding) {
    ValidationResult result;
    result.checkName = "BundleFitValidation";
    result.unit = "mm";
    result.mandatory = true;

    if (winding.bundleFitStatus != EvaluationStatus::Evaluated) {
        result.status = EvaluationStatus::NotEvaluated;
        result.passed = false;
        result.calculatedValue = 0.0;
        result.explanation = "not evaluated: " + std::string(winding.parallelStrands > 1 ? "this core has no real window width/height (toroid, or a two-piece core missing that data)" : "single-strand winding - no parallel-strand bundle to check a linear opening against");
        return result;
    }

    result.calculatedValue = winding.bundleWidthMm;
    result.limitValue = winding.narrowestWindowOpeningMm;
    result.passed = winding.bundleFitsWindowOpening;
    result.explanation = std::to_string(winding.parallelStrands) + "x AWG bundle, " + std::to_string(winding.bundleWidthMm) + " mm wide (strands laid side by side) vs narrowest real window opening " + std::to_string(winding.narrowestWindowOpeningMm) + " mm - does not account for bundle twisting/multi-row arrangement, which would need real winding-pattern data this project doesn't have";
    return result;
}

//precondition: none
//postcondition: passes only when thermal.status == PreliminaryThermalEstimate and the predicted rise is within allowableTempRiseC; otherwise not_evaluated (passed=false, never an assumed pass - spec section 10). ThermalStatus has no "fully evaluated" value
//(see ThermalEvaluation.h), so a passing result here always carries isPreliminaryEstimate=true - the numeric check genuinely ran and passed/failed, but rests on a Phase 1 coarse thermal-resistance constant, never per-core measured/simulated data.
ValidationResult ThermalValidation(const ThermalEvaluationResult& thermal, double allowableTempRiseC, bool turnsConverged) {
    ValidationResult result;
    result.checkName = "ThermalValidation";
    result.unit = "C";
    result.limitValue = allowableTempRiseC;

    /*
    a real user report: on a non-converged design, the thermal loop still runs downstream against turns=0 (essentially no winding), which makes copper loss and therefore predicted rise trivially
    near-zero - reading as a real, comfortable PASS for a design that doesn't exist. Report NotEvaluated instead, ahead of the thermal.status check below (which would otherwise happily
    report this trivial "pass" as a genuine PreliminaryThermalEstimate).
    */
    if (!turnsConverged) {
        result.status = EvaluationStatus::NotEvaluated;
        result.passed = false;
        result.calculatedValue = 0.0;
        result.explanation = "not evaluated: turns/gap design did not converge - there is no real winding to model thermal rise for";
        return result;
    }

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
    result.explanation = "predicted temperature rise " + std::to_string(thermal.predictedTempRiseC) + " C vs allowable " + std::to_string(allowableTempRiseC) + " C (Rth=" + std::to_string(thermal.thermalResistanceCPerWUsed) + " C/W is " + (thermal.thermalResistanceIsGeometryDerived ? "a Phase 1 size-aware estimate derived from this core's own Ae/Le geometry via Newton's law of cooling, not measured or simulated data" : "a Phase 1 flat default - this core's geometry was unavailable to derive a size-aware estimate") + ")";
    return result;
}

//precondition: none
//postcondition: returns the four named flux-limit tiers (spec section 3) for the given material/rules, with the two permanently-not-evaluated tiers always NotEvaluated regardless of which material/rules are passed in.
FluxLimitTiers calculateFluxLimitTiers(const MaterialCandidate& material, const DesignRules& rules) {
    FluxLimitTiers tiers;
    FluxLimit limit = applicableFluxLimit(material, rules);
    tiers.absoluteSaturationT = limit.limitT;
    tiers.absoluteSaturationIsDefault = limit.usedDefault;
    tiers.recommendedOperatingT = limit.limitT * rules.recommendedFluxDerateFactor;
    return tiers;
}
