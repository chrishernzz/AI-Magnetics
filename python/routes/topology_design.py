#type: ignore
"""
topology_design.py

MODE 1: derives InductorDesignRequest fields from converter-level
operating requirements, for engineers who know their converter but not
their inductor's requirements yet. POST /topology-design/buck is the only
implemented topology (V1 scope, see BuckElectricalSolver.h) - adding a
second topology means adding one more route function calling one more
C++ solver, not changing this one.

The response mirrors InductorDesignRequest's own field names, so the
frontend can display it as a "Derived Magnetic Requirements" preview and
then feed it straight into the existing /inductor-design flow (see
inductor_design.py) - Mode 1 and Mode 2 converge at that point and share
every stage after it.
"""
from typing import Optional

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

import magnetics_cpp

router = APIRouter()


class BuckTopologyInput(BaseModel):
    vinMinV: float
    vinMaxV: float
    voutV: float
    ioutA: float
    switchingFreqKHz: float

    #target inductor ripple current as a percentage of ioutA (e.g. 20 for 20%). Sizing uses vinMaxV as the worst-case point - see BuckElectricalSolver.h.
    rippleCurrentPercent: float

    ambientTemperatureC: float
    allowableTempRiseC: float

    #falls back to DesignRules.defaultInductanceTolerancePercent (applied downstream by RequirementDerivationService) if omitted.
    inductanceTolerancePercent: Optional[float] = None

#precondition: request is a valid BuckTopologyInput object and required fields have already passed pydantic validation
#postcondition: returns a populated magnetics_cpp.TopologyInput and all matching fields are copied from python to c++ object
def build_cpp_input(request: BuckTopologyInput) -> "magnetics_cpp.TopologyInput":
    cpp_input = magnetics_cpp.TopologyInput()
    cpp_input.vinMinV = request.vinMinV
    cpp_input.vinMaxV = request.vinMaxV
    cpp_input.voutV = request.voutV
    cpp_input.ioutA = request.ioutA
    cpp_input.switchingFreqKHz = request.switchingFreqKHz
    cpp_input.rippleCurrentPercent = request.rippleCurrentPercent
    cpp_input.ambientTemperatureC = request.ambientTemperatureC
    cpp_input.allowableTempRiseC = request.allowableTempRiseC
    cpp_input.inductanceTolerancePercent = request.inductanceTolerancePercent
    return cpp_input

#precondition: r is a valid InductorDesignRequest embedded in a BuckSolveResult
#postcondition: returns a json-serializable dictionary and returned fields match InductorDesignRequest naming
def _serialize_derived_request(r) -> dict:
    return {
        "inductanceUH": r.inductanceUH,
        "peakCurrentA": r.peakCurrentA,
        "switchingFreqKHz": r.switchingFreqKHz,
        "ambientTemperatureC": r.ambientTemperatureC,
        "allowableTempRiseC": r.allowableTempRiseC,
        "inductanceTolerancePercent": r.inductanceTolerancePercent,
        #rmsCurrentA is intentionally left unset by BuckElectricalSolver - averageCurrentA + rippleCurrentPeakToPeakA are the real derived values, and RequirementDerivationService derives RMS from them
        #downstream using the one triangular-ripple formula in the codebase (see BuckElectricalSolver.h for why).
        "averageCurrentA": r.averageCurrentA,
        "rippleCurrentPeakToPeakA": r.rippleCurrentPeakToPeakA,
    }

#precondition: p is a valid BuckOperatingPointResult
#postcondition: returns a json-serializable dictionary of the real electrical quantities at this specific Vin
def _serialize_operating_point(p) -> dict:
    return {
        "vinV": p.vinV,
        "dutyCycle": p.dutyCycle,
        "rippleCurrentPeakToPeakA": p.rippleCurrentPeakToPeakA,
        "peakCurrentA": p.peakCurrentA,
        "minInductorCurrentA": p.minInductorCurrentA,
    }

#precondition: r is a valid BuckSolveResult returned from solve_buck_topology
#postcondition: returns a json-serializable dictionary - the derived request, both operating points, which point is
#worst per quantity, CCM/DCM classification, and the stated Phase 1 assumptions/warnings
def _serialize_buck_solve_result(r) -> dict:
    return {
        "request": _serialize_derived_request(r.request),
        "atVinMin": _serialize_operating_point(r.atVinMin),
        "atVinMax": _serialize_operating_point(r.atVinMax),
        "worstCaseDutyCycleAt": r.worstCaseDutyCycleAt,
        "worstCaseRippleAt": r.worstCaseRippleAt,
        "worstCasePeakAt": r.worstCasePeakAt,
        "conductionMode": r.conductionMode.name,
        "assumptions": list(r.assumptions),
        "warnings": list(r.warnings),
    }

#precondition: request body matches BuckTopologyInput schema: vinMinV, vinMaxV, voutV, ioutA, switchingFreqKHz, and rippleCurrentPercent are provided input values represent a physically valid buck converter and magnetics_cpp topology solver is available
#postcondition: derived magnetic requirements (at both Vin_min and Vin_max) are returned, and invalid physical inputs return http 422
@router.post("/topology-design/buck")
def topology_design_buck(request: BuckTopologyInput) -> dict:
    cpp_input = build_cpp_input(request)
    rules = magnetics_cpp.design_rules_phase1_default()
    try:
        derived = magnetics_cpp.solve_buck_topology(cpp_input, rules)
    except ValueError as exc:
        #BuckElectricalSolver raises std::invalid_argument for physically invalid inputs (e.g. Vout >= Vin_min, or DCM at Vin_max) - pybind11 surfaces this as a Python ValueError.
        raise HTTPException(status_code=422, detail=str(exc))

    return _serialize_buck_solve_result(derived)
