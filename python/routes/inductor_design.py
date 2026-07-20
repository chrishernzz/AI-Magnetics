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
request and returns one explainable DesignRecommendation. The old
single-stage endpoints (/material-selection, /calculate, /core-selection,
/turns-calculation) and the single-pick C++ logic behind them
(MaterialSelection.cpp, CoreSelection.cpp, MaterialSelectionService,
CoreSelectionService) have been removed - they only ever re-ran earlier
stages redundantly and, in CoreSelection.cpp's case, used to silently
fall back to an oversized core when nothing actually fit.
"""
from typing import Optional

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

import magnetics_cpp

router = APIRouter()


class InductorDesignRequest(BaseModel):
    # Required direct inputs (spec section 5)
    inductanceUH: float
    peakCurrentA: float
    switchingFreqKHz: float
    ambientTemperatureC: float
    allowableTempRiseC: float

    # Required in principle, but may be omitted if it can be derived from
    # averageCurrentA + rippleCurrentPeakToPeakA (triangular ripple only -
    # see RequirementDerivationService). Never inferred from peakCurrentA.
    rmsCurrentA: Optional[float] = None

    # Falls back to DesignRules.defaultInductanceTolerancePercent if omitted.
    inductanceTolerancePercent: Optional[float] = None

    # Optional direct inputs (spec section 5) - prepared for, not all
    # consumed by every stage yet.
    averageCurrentA: Optional[float] = None
    rippleCurrentPeakToPeakA: Optional[float] = None
    maximumDcrMilliOhm: Optional[float] = None
    maximumWidthMm: Optional[float] = None
    maximumHeightMm: Optional[float] = None
    maximumLengthMm: Optional[float] = None
    preferredMaterialFamily: Optional[str] = None
    preferredCoreGeometry: Optional[str] = None


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
    }


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
    }


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
    }


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
    }


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


def _serialize_thermal(t) -> dict:
    return {
        "status": t.status.name,
        "predictedTempRiseC": t.predictedTempRiseC,
        "missingDataExplanation": t.missingDataExplanation,
    }


def _serialize_rejection(r) -> dict:
    return {"checkName": r.checkName, "explanation": r.explanation}


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
    }


def _serialize_rules(r) -> dict:
    return {
        "windowUtilization": r.windowUtilization,
        "allowableCurrentDensityAperCm2": r.allowableCurrentDensityAperCm2,
        "defaultFluxDensityLimitT": r.defaultFluxDensityLimitT,
        "minimumSaturationMarginPercent": r.minimumSaturationMarginPercent,
        "maximumFillFactor": r.maximumFillFactor,
        "defaultInductanceTolerancePercent": r.defaultInductanceTolerancePercent,
        "minimumSingleStrandAwg": r.minimumSingleStrandAwg,
    }


def serialize_recommendation(rec) -> dict:
    return {
        "status": rec.status,
        "message": rec.message,
        "candidates": [_serialize_candidate(c) for c in rec.candidates],
        "rejectedCandidates": [_serialize_candidate(c) for c in rec.rejectedCandidates],
        "activeRules": _serialize_rules(rec.activeRules),
        "requiredAreaProductCm4": rec.requiredAreaProductCm4,
        "largestAvailableAreaProductCm4": rec.largestAvailableAreaProductCm4,
    }


@router.post("/inductor-design")
def inductor_design(request: InductorDesignRequest) -> dict:
    cpp_request = build_cpp_request(request)
    try:
        recommendation = magnetics_cpp.run_inductor_design(cpp_request)
    except ValueError as exc:
        # RequirementDerivationService raises std::invalid_argument (e.g.
        # rmsCurrentA not supplied and not derivable) - pybind11 surfaces
        # this as a Python ValueError.
        raise HTTPException(status_code=422, detail=str(exc))

    return serialize_recommendation(recommendation)
