#type: ignore
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fastapi import APIRouter
from pydantic import BaseModel
import magnetics_cpp

router = APIRouter()

#precondition: None
#postcondition: user will now be able to get a response based on these class
class BuckInput(BaseModel):
    inductanceUH: float
    peakCurrentA: float
    switchingFreqKHz: float
    allowableTempRiseC: float

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

def build_material_input(request: BuckInput):
    input_data = magnetics_cpp.MaterialSelectionInput()
    input_data.switchingFreqHz = request.switchingFreqKHz * 1000.0
    return input_data

def build_area_product_input(request: BuckInput):
    input_data = magnetics_cpp.AreaProductInput()
    input_data.inductanceH = request.inductanceUH * 1e-6
    input_data.peakCurrentA = request.peakCurrentA
    input_data.switchingFreqHz = request.switchingFreqKHz * 1000.0
    input_data.allowableTempRiseC = request.allowableTempRiseC
    input_data.windowUtilization = 0.4
    input_data.fluxDensityT = 0.30
    input_data.currentDensityAPerCm2 = 400.0
    return input_data


@router.post("/material-selection", response_model=MaterialSelectionResponse)
def material_selection(request: BuckInput) -> MaterialSelectionResponse:
    service = magnetics_cpp.MaterialSelectionService()
    input_data = build_material_input(request)
    result = service.calculate(input_data)

    return MaterialSelectionResponse(
        materialFamily=result.materialFamily,
        muOpt=result.muOpt,
        reason=result.reason,
        alternatives=result.alternatives,
    )
@router.post("/calculate", response_model=AreaProductResponse)
def calculate(request: BuckInput) -> AreaProductResponse:
    input_data = build_area_product_input(request)
    area_product = magnetics_cpp.calculate_ap(input_data)
    energy = magnetics_cpp.calculate_stored_energy(
        input_data.inductanceH, input_data.peakCurrentA
    )

    return AreaProductResponse(areaProduct=area_product, energy=energy)
@router.post("/core-selection", response_model=CoreSelectionResponse)
def core_selection(request: BuckInput) -> CoreSelectionResponse:
    material_result = material_selection(request)

    input_data = magnetics_cpp.CoreSelectionInput()
    input_data.areaProduct = magnetics_cpp.calculate_ap(build_area_product_input(request))
    input_data.peakCurrentA = request.peakCurrentA
    input_data.recommendedMaterial = material_result.materialFamily

    service = magnetics_cpp.CoreSelectionService()
    result = service.calculate(input_data)

    return CoreSelectionResponse(partNumber=result.partNumber, material=result.material, mu=result.mu, al=result.al, ae=result.ae, wa=result.wa,le=result.le,)
@router.post("/turns-calculation", response_model=TurnsCalculationResponse)
def turns_calculation(request: BuckInput) -> TurnsCalculationResponse:
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