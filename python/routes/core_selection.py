#type: ignore
"""
core_selection.py

DEPRECATED: these four endpoints predate the Phase 1 inductor design engine
and are kept only for backward compatibility. Each one runs a single,
legacy stage of the pipeline in isolation (and, for anything past
material selection, silently re-runs every upstream stage internally on
every call). None of them perform turns/gap convergence, magnetic
validation, winding design, or loss evaluation - use POST /inductor-design
(python/routes/inductor_design.py) for the real Phase 1 pipeline, which
runs the full call graph exactly once and returns one explainable result.

InductorDesignRequest (formerly BuckInput - spec section 6 renamed it,
since every field here is a direct inductor specification with no
topology-specific knowledge) is defined once, in inductor_design.py, and
imported here so both old and new routes share exactly one request model.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fastapi import APIRouter
from pydantic import BaseModel
import magnetics_cpp

from routes.inductor_design import InductorDesignRequest

router = APIRouter()


class MaterialSelectionResponse(BaseModel):
    materialFamily: str
    muOpt: float
    reason: str
    alternatives: str

class AreaProductResponse(BaseModel):
    areaProduct: float
    energy: float

class CoreSelectionResponse(BaseModel):
    partNumber: str
    material: str
    mu: float
    al: float
    ae: float
    wa: float
    le: float

class TurnsCalculationResponse(BaseModel):
    turns: int
    inductanceUH: float
    al: float

def build_material_input(request: InductorDesignRequest):
    input_data = magnetics_cpp.MaterialSelectionInput()
    input_data.switchingFreqHz = request.switchingFreqKHz * 1000.0
    return input_data

def build_area_product_input(request: InductorDesignRequest):
    # Ku/Bmax/J come from the same named Phase 1 ruleset the real
    # /inductor-design pipeline uses (src/rules/DesignRules.cpp) - no
    # magnetic-design constant is hard-coded in this route (spec section 7).
    rules = magnetics_cpp.design_rules_phase1_default()

    input_data = magnetics_cpp.AreaProductInput()
    input_data.inductanceH = request.inductanceUH * 1e-6
    input_data.peakCurrentA = request.peakCurrentA
    input_data.switchingFreqHz = request.switchingFreqKHz * 1000.0
    input_data.allowableTempRiseC = request.allowableTempRiseC
    input_data.windowUtilization = rules.windowUtilization
    input_data.fluxDensityT = rules.defaultFluxDensityLimitT
    input_data.currentDensityAPerCm2 = rules.allowableCurrentDensityAperCm2
    return input_data

#Material Selction: POST (deprecated - see module docstring)
@router.post("/material-selection", response_model=MaterialSelectionResponse, deprecated=True)
def material_selection(request: InductorDesignRequest) -> MaterialSelectionResponse:
    service = magnetics_cpp.MaterialSelectionService()
    input_data = build_material_input(request)
    result = service.calculate(input_data)

    return MaterialSelectionResponse(
        materialFamily=result.materialFamily,
        muOpt=result.muOpt,
        reason=result.reason,
        alternatives=result.alternatives,
    )
#Ap calculation: POST (deprecated - see module docstring)
@router.post("/calculate", response_model=AreaProductResponse, deprecated=True)
def calculate(request: InductorDesignRequest) -> AreaProductResponse:
    input_data = build_area_product_input(request)
    area_product = magnetics_cpp.calculate_ap(input_data)
    energy = magnetics_cpp.calculate_stored_energy(
        input_data.inductanceH, input_data.peakCurrentA
    )

    return AreaProductResponse(areaProduct=area_product, energy=energy)
#Core Selction: POST (deprecated - see module docstring)
@router.post("/core-selection", response_model=CoreSelectionResponse, deprecated=True)
def core_selection(request: InductorDesignRequest) -> CoreSelectionResponse:
    material_result = material_selection(request)

    input_data = magnetics_cpp.CoreSelectionInput()
    input_data.areaProduct = magnetics_cpp.calculate_ap(build_area_product_input(request))
    input_data.peakCurrentA = request.peakCurrentA
    input_data.recommendedMaterial = material_result.materialFamily

    service = magnetics_cpp.CoreSelectionService()
    result = service.calculate(input_data)

    return CoreSelectionResponse(partNumber=result.partNumber, material=result.material, mu=result.mu, al=result.al, ae=result.ae, wa=result.wa,le=result.le,)
#Turns calculation: POST (deprecated - see module docstring; legacy AL-only formula, no gap iteration)
@router.post("/turns-calculation", response_model=TurnsCalculationResponse, deprecated=True)
def turns_calculation(request: InductorDesignRequest) -> TurnsCalculationResponse:
    core_result = core_selection(request)
    core_input = magnetics_cpp.CoreSelectionResult()

    core_input.partNumber = \
        core_result.partNumber

    core_input.material = \
        core_result.material

    core_input.mu = \
        core_result.mu

    core_input.al = \
        core_result.al

    core_input.ae = \
        core_result.ae

    core_input.wa = \
        core_result.wa

    core_input.le = \
        core_result.le

    turns_input = magnetics_cpp.TurnsCalculationInput()

    turns_input.inductanceUH = \
        request.inductanceUH

    turns_input.core = \
        core_input

    result = magnetics_cpp.calculate_turns(turns_input)

    return TurnsCalculationResponse(turns=result.turns, inductanceUH=result.inductanceUH, al=result.al)
