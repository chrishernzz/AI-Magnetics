#type: ignore
"""
inductor_design.py

The Phase 1 direct-entry inductor design pipeline. InductorDesignRequest is
the renamed successor to the old shared BuckInput model - the old name
described this as a buck-specific input even though every field is a
direct inductor specification with no topology knowledge (spec section 6).

POST /inductor-design is the single entry point: it runs the full C++
pipeline (materials -> area product -> cores -> turns/gap -> magnetic
validation -> winding -> losses -> thermal -> ranking) exactly once per
request and returns one explainable DesignRecommendation.
"""
from typing import Optional

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

import magnetics_cpp

router = APIRouter()


class InductorDesignRequest(BaseModel):
    #required direct inputs (spec section 5)
    inductanceUH: float
    peakCurrentA: float
    switchingFreqKHz: float
    ambientTemperatureC: float
    allowableTempRiseC: float

    #required in principle, but may be omitted if it can be derived from
    #averageCurrentA + rippleCurrentPeakToPeakA (triangular ripple only -
    #see RequirementDerivationService). Never inferred from peakCurrentA.
    rmsCurrentA: Optional[float] = None

    #falls back to DesignRules.defaultInductanceTolerancePercent if omitted.
    inductanceTolerancePercent: Optional[float] = None

    #optional direct inputs (spec section 5) - prepared for, not all consumed by every stage yet.
    averageCurrentA: Optional[float] = None
    rippleCurrentPeakToPeakA: Optional[float] = None
    maximumDcrMilliOhm: Optional[float] = None
    maximumWidthMm: Optional[float] = None
    maximumHeightMm: Optional[float] = None
    maximumLengthMm: Optional[float] = None
    preferredMaterialFamily: Optional[str] = None
    preferredCoreGeometry: Optional[str] = None

#precondition: request is a valid InductorDesignRequest object and required fields have already passed pydantic validation
#postcondition: returns a populated magnetics_cpp.InductorDesignRequest and all matching fields are copied from python to c++ object
def build_cpp_request(request: InductorDesignRequest) -> "magnetics_cpp.InductorDesignRequest":
    cpp_request = magnetics_cpp.InductorDesignRequest()
    cpp_request.inductanceUH = request.inductanceUH
    cpp_request.peakCurrentA = request.peakCurrentA
    cpp_request.switchingFreqKHz = request.switchingFreqKHz
    cpp_request.ambientTemperatureC = request.ambientTemperatureC
    cpp_request.allowableTempRiseC = request.allowableTempRiseC
    cpp_request.rmsCurrentA = request.rmsCurrentA
    cpp_request.inductanceTolerancePercent = request.inductanceTolerancePercent
    cpp_request.averageCurrentA = request.averageCurrentA
    cpp_request.rippleCurrentPeakToPeakA = request.rippleCurrentPeakToPeakA
    cpp_request.maximumDcrMilliOhm = request.maximumDcrMilliOhm
    cpp_request.maximumWidthMm = request.maximumWidthMm
    cpp_request.maximumHeightMm = request.maximumHeightMm
    cpp_request.maximumLengthMm = request.maximumLengthMm
    cpp_request.preferredMaterialFamily = request.preferredMaterialFamily
    cpp_request.preferredCoreGeometry = request.preferredCoreGeometry
    return cpp_request

#precondition: v is a validation object from magnetics_cpp and v contains all expected validation fields
#postcondition: returns a json-serializable dictionary and enum values are converted into strings
def _serialize_validation(v) -> dict:
    return {
        "passed": v.passed,
        "checkName": v.checkName,
        "calculatedValue": v.calculatedValue,
        "limitValue": v.limitValue,
        "unit": v.unit,
        "explanation": v.explanation,
        "usedDefaultLimit": v.usedDefaultLimit,
        "status": v.status.name,
    }

#precondition: s is a valid SourceInfo object
#postcondition: returns a serializable source/provenance dictionary - datasheetRevision/Url/dateAccessed are
#always None in Phase 1 (no such data exists in the current snapshot), never fabricated here.
def _serialize_source_info(s) -> dict:
    return {
        "manufacturer": s.manufacturer,
        "partNumber": s.partNumber,
        "materialGrade": s.materialGrade,
        "datasheetName": s.datasheetName,
        "datasheetRevision": s.datasheetRevision,
        "datasheetUrl": s.datasheetUrl,
        "dateAccessed": s.dateAccessed,
        "confidence": s.confidence.name,
        "confidenceNote": s.confidenceNote,
    }

#precondition: h is a valid HardwareValidationRecord object
#postcondition: returns a serializable dictionary - every field is None and status is "not_measured" in Phase 1
def _serialize_hardware_validation(h) -> dict:
    return {
        "measuredInductanceUH": h.measuredInductanceUH,
        "measuredDcrMilliOhm": h.measuredDcrMilliOhm,
        "measuredRippleCurrentA": h.measuredRippleCurrentA,
        "measuredPeakCurrentA": h.measuredPeakCurrentA,
        "measuredCoreTempC": h.measuredCoreTempC,
        "measuredWindingTempC": h.measuredWindingTempC,
        "predictedVsMeasuredInductanceErrorPercent": h.predictedVsMeasuredInductanceErrorPercent,
        "predictedVsMeasuredDcrErrorPercent": h.predictedVsMeasuredDcrErrorPercent,
        "testNotes": h.testNotes,
        "testDate": h.testDate,
        "status": h.status,
    }

#precondition: v is a valid EngineVersions object
#postcondition: returns a serializable versions dictionary
def _serialize_versions(v) -> dict:
    return {
        "calculationEngineVersion": v.calculationEngineVersion,
        "designRulesVersion": v.designRulesVersion,
        "coreDatabaseVersion": v.coreDatabaseVersion,
        "materialDatabaseVersion": v.materialDatabaseVersion,
    }

#precondition: m is a valid material result object and material selection stage has completed
#postcondition: returns a serializable material dictionary and missing data warnings are converted to python lists
def _serialize_material(m) -> dict:
    return {
        "materialFamily": m.materialFamily,
        "muOpt": m.muOpt,
        "frequencySuitable": m.frequencySuitable,
        "hasBmaxData": m.hasBmaxData,
        "hasCoreLossData": m.hasCoreLossData,
        "bmaxT": m.bmaxT,
        "cuLossFactor": m.cuLossFactor,
        "reason": m.reason,
        "alternatives": m.alternatives,
        "missingDataWarnings": list(m.missingDataWarnings),
        "source": _serialize_source_info(m.source),
    }

#precondition: c is a valid core-selection object and core search stage has completed
#postcondition: returns a serializable core dictionary and geometric and magnetic properties are preserved
def _serialize_core(c) -> dict:
    return {
        "partNumber": c.partNumber,
        "material": c.material,
        "mu": c.mu,
        "al": c.al,
        "aeMm2": c.aeMm2,
        "waMm2": c.waMm2,
        "leMm": c.leMm,
        "mltMm": c.mltMm,
        "areaProductCm4": c.areaProductCm4,
        "meetsAreaProduct": c.meetsAreaProduct,
        "vendor": c.vendor,
        "source": _serialize_source_info(c.source),
    }

#precondition: turns and gap calculations have completed and t contains calculated winding parameters
#postcondition: returns a serializable turns/gap dictionary, convergence and tolerance information are preserved
def _serialize_turns_and_gap(t) -> dict:
    return {
        "turns": t.turns,
        "gapMm": t.gapMm,
        "effectiveAlNHPerTurnSquared": t.effectiveAlNHPerTurnSquared,
        "calculatedInductanceUH": t.calculatedInductanceUH,
        "inductanceErrorPercent": t.inductanceErrorPercent,
        "withinTolerance": t.withinTolerance,
        "converged": t.converged,
        "rejectionReasons": list(t.rejectionReasons),
        "gapMethod": t.gapMethod.name,
        "gapMinMm": t.gapMinMm,
        "gapMaxMm": t.gapMaxMm,
        "inductanceAtMinGapUH": t.inductanceAtMinGapUH,
        "inductanceAtMaxGapUH": t.inductanceAtMaxGapUH,
        "inductanceWithinToleranceAcrossGapRange": t.inductanceWithinToleranceAcrossGapRange,
        "smallGapWarning": t.smallGapWarning,
        "smallGapWarningReason": t.smallGapWarningReason,
    }

#precondition: winding design stage has completed and w contains winding analysis data
#postcondition: returns a serializable winding dictionary and resistance status enum is converted to string
def _serialize_winding(w) -> dict:
    return {
        "wireDescription": w.wireDescription,
        "conductorAreaMm2": w.conductorAreaMm2,
        "parallelStrands": w.parallelStrands,
        "fillFactor": w.fillFactor,
        "currentDensityAperMm2": w.currentDensityAperMm2,
        "fitsWindow": w.fitsWindow,
        "resistanceStatus": w.resistanceStatus.name,
        "totalWireLengthM": w.totalWireLengthM,
        "dcrOhms": w.dcrOhms,
        "missingData": list(w.missingData),
        "constructionType": w.constructionType.name,
        "insulatedConductorDiameterMm": w.insulatedConductorDiameterMm,
        "insulatedConductorAreaMm2": w.insulatedConductorAreaMm2,
        "physicalDescription": w.physicalDescription,
        "physicalWindowAreaMm2": w.physicalWindowAreaMm2,
        "physicalWindowFillFactor": w.physicalWindowFillFactor,
        "fitsPhysicalWindow": w.fitsPhysicalWindow,
        "effectiveCurrentDensityAperMm2": w.effectiveCurrentDensityAperMm2,
        "bundleFitStatus": w.bundleFitStatus.name,
        "coreWindingLengthM": w.coreWindingLengthM,
        "leadLengthM": w.leadLengthM,
        "routingLengthM": w.routingLengthM,
        "totalLengthM": w.totalLengthM,
        "connectionResistanceOhms": w.connectionResistanceOhms,
        "coldDcrOhmsAt20C": w.coldDcrOhmsAt20C,
        "estimatedHotDcrOhms": w.estimatedHotDcrOhms,
    }

#precondition: loss calculations have completed and l contains copper, core, and hf losses
#postcondition: returns a serializable loss dictionary and status enums are converted to strings
def _serialize_losses(l) -> dict:
    return {
        "copperLossStatus": l.copperLossStatus.name,
        "copperLossW": l.copperLossW,
        "coreLossStatus": l.coreLossStatus.name,
        "coreLossW": l.coreLossW,
        "highFrequencyLossStatus": l.highFrequencyLossStatus.name,
        "highFrequencyLossW": l.highFrequencyLossW,
        "missingData": list(l.missingData),
    }

#precondition: thermal stage has completed and t contains predicted thermal results
#postcondition: returns a serializable thermal dictionary and status enum is converted to string
def _serialize_thermal(t) -> dict:
    return {
        "status": t.status.name,
        "predictedTempRiseC": t.predictedTempRiseC,
        "missingDataExplanation": t.missingDataExplanation,
    }

#precondition: r is a valid rejection reason object and rejection analysis has completed
#postcondition: returns a serializable rejection dictionary and rejection explanation is preserved
def _serialize_rejection(r) -> dict:
    return {"checkName": r.checkName, "explanation": r.explanation}

#precondition: f is a valid FluxLimitTiers object
#postcondition: returns a serializable flux-limit-tier dictionary; temperatureAdjustedStatus/coreLossLimitedStatus are always NotEvaluated in Phase 1 - see DesignValidation.h
def _serialize_flux_limit_tiers(f) -> dict:
    return {
        "absoluteSaturationT": f.absoluteSaturationT,
        "absoluteSaturationIsDefault": f.absoluteSaturationIsDefault,
        "recommendedOperatingT": f.recommendedOperatingT,
        "temperatureAdjustedStatus": f.temperatureAdjustedStatus.name,
        "temperatureAdjustedExplanation": f.temperatureAdjustedExplanation,
        "coreLossLimitedStatus": f.coreLossLimitedStatus.name,
        "coreLossLimitedExplanation": f.coreLossLimitedExplanation,
    }

#precondition: c is a candidate generated by the c++ design engine and all design pipeline stages have produced results
#postcondition: returns a fully serialized candidate dictionary and all nested objects are recursively serialized
def _serialize_candidate(c) -> dict:
    return {
        "material": _serialize_material(c.material),
        "core": _serialize_core(c.core),
        "turnsAndGap": _serialize_turns_and_gap(c.turnsAndGap),
        "validations": [_serialize_validation(v) for v in c.validations],
        "winding": _serialize_winding(c.winding),
        "losses": _serialize_losses(c.losses),
        "thermal": _serialize_thermal(c.thermal),
        "passed": c.passed,
        "rejectionReasons": [_serialize_rejection(r) for r in c.rejectionReasons],
        "hardwareValidation": _serialize_hardware_validation(c.hardwareValidation),
        "fluxLimits": _serialize_flux_limit_tiers(c.fluxLimits),
    }

#precondition: r contains valid design rule values and rule configuration has been loaded
#postcondition: returns a serializable rule dictionary and all active rule thresholds are preserved
def _serialize_rules(r) -> dict:
    return {
        "windowUtilization": r.windowUtilization,
        "allowableCurrentDensityAperCm2": r.allowableCurrentDensityAperCm2,
        "defaultFluxDensityLimitT": r.defaultFluxDensityLimitT,
        "minimumSaturationMarginPercent": r.minimumSaturationMarginPercent,
        "maximumFillFactor": r.maximumFillFactor,
        "defaultInductanceTolerancePercent": r.defaultInductanceTolerancePercent,
        "minimumSingleStrandAwg": r.minimumSingleStrandAwg,
        "maximumRippleCurrentPercent": r.maximumRippleCurrentPercent,
        "recommendedFluxDerateFactor": r.recommendedFluxDerateFactor,
        "minManufacturableGapMm": r.minManufacturableGapMm,
        "gapStepMm": r.gapStepMm,
        "maxGapFraction": r.maxGapFraction,
        "gapTolerancePercent": r.gapTolerancePercent,
        "gapMethod": r.gapMethod.name,
        "singleBuildInsulationBuildUpMm": r.singleBuildInsulationBuildUpMm,
        "packingFactor": r.packingFactor,
        "bobbinWindowDerateFactor": r.bobbinWindowDerateFactor,
        "marginAllowanceAreaFraction": r.marginAllowanceAreaFraction,
        "leadExitAllowanceAreaFraction": r.leadExitAllowanceAreaFraction,
        "currentSharingDerateFactor": r.currentSharingDerateFactor,
        "totalLeadLengthAllowanceMm": r.totalLeadLengthAllowanceMm,
        "routingLengthAllowanceMm": r.routingLengthAllowanceMm,
        "connectionResistanceMilliOhm": r.connectionResistanceMilliOhm,
        "copperTempCoefficientPerC": r.copperTempCoefficientPerC,
        "assumedWindingTempCWhenThermalNotEvaluated": r.assumedWindingTempCWhenThermalNotEvaluated,
        "defaultThermalResistanceCPerW": r.defaultThermalResistanceCPerW,
        "thermalConvergenceThresholdC": r.thermalConvergenceThresholdC,
        "maxThermalIterations": r.maxThermalIterations,
        "skinDepthRiskModerateThreshold": r.skinDepthRiskModerateThreshold,
        "skinDepthRiskHighThreshold": r.skinDepthRiskHighThreshold,
    }

#precondition: rec is a completed design recommendation and the c++ pipeline executed successfully
#postcondition: returns a fully json-serializable respones, all candidates, rejected candidates, and rules are included
def serialize_recommendation(rec) -> dict:
    return {
        "status": rec.status,
        "message": rec.message,
        "candidates": [_serialize_candidate(c) for c in rec.candidates],
        "rejectedCandidates": [_serialize_candidate(c) for c in rec.rejectedCandidates],
        "activeRules": _serialize_rules(rec.activeRules),
        "requiredAreaProductCm4": rec.requiredAreaProductCm4,
        "largestAvailableAreaProductCm4": rec.largestAvailableAreaProductCm4,
        "versions": _serialize_versions(rec.versions),
    }

#precondition: request body matches InductorDesignRequest scheme, required electrical parameters are provided, and c++ design engine is available and initialized
#postcondition: exactly one full design pipeline execution occurs, successful execution returns a serialized recommendation, and invalid derived requirements return http 422
@router.post("/inductor-design")
def inductor_design(request: InductorDesignRequest) -> dict:
    #builds the request in c++
    cpp_request = build_cpp_request(request)
    try:
        recommendation = magnetics_cpp.run_inductor_design(cpp_request)
    except ValueError as exc:
        #requirementDerivationService raises std::invalid_argument (e.g. rmsCurrentA not supplied and not derivable) - pybind11 surfaces this as a Python ValueError.
        raise HTTPException(status_code=422, detail=str(exc))

    return serialize_recommendation(recommendation)
