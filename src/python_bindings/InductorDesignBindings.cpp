#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "backend/services/InductorDesignService.h"
#include "core/sizing/AreaProduct.h"
#include "core/sizing/CoreEvaluation.h"
#include "core/model/DesignRecommendation.h"
#include "core/model/InductorDesignRequest.h"
#include "core/losses/LossEvaluation.h"
#include "core/sizing/MaterialEvaluation.h"
#include "core/model/RejectionReason.h"
#include "core/thermal/ThermalEvaluation.h"
#include "core/magnetics/TurnsAndGapDesign.h"
#include "core/winding/WindingDesign.h"
#include "data/CoreDatabase.h"
#include "data/Materials.h"
#include "rules/DesignRules.h"
#include "validation/EvaluationStatus.h"
#include "validation/Validation.h"

namespace py = pybind11;

PYBIND11_MODULE(magnetics_cpp, m) {
    m.doc() = "AIMagnetics C++ bindings for Python";

    // ============================================================
    // Area product / stored energy - shared by the Phase 1 pipeline
    // (called internally in C++ by InductorDesignService) and by
    // tests/python/test_unit_conversions.py, which calls these directly
    // to verify the formula independently of the full pipeline.
    // ============================================================

    py::class_<AreaProductInput>(m, "AreaProductInput")
        .def(py::init<>())
        .def_readwrite("inductanceH", &AreaProductInput::inductanceH)
        .def_readwrite("peakCurrentA", &AreaProductInput::peakCurrentA)
        .def_readwrite("switchingFreqHz", &AreaProductInput::switchingFreqHz)
        .def_readwrite("allowableTempRiseC", &AreaProductInput::allowableTempRiseC)
        .def_readwrite("windowUtilization", &AreaProductInput::windowUtilization)
        .def_readwrite("fluxDensityT", &AreaProductInput::fluxDensityT)
        .def_readwrite("currentDensityAPerCm2", &AreaProductInput::currentDensityAPerCm2);

    m.def("calculate_ap", &calculateAp, "Calculate the area product (Ap) from input");
    m.def("calculate_stored_energy", &calculateStoredEnergy, "Calculate stored energy for the given inductance and current");

    // Raw data structs, populated once at FastAPI startup with real data
    // (see python/app.py) - both Materials and Cores are loaded here.
    py::class_<CoreData>(m, "CoreData")
        .def(py::init<>())
        .def_readwrite("partNumber", &CoreData::partNumber)
        .def_readwrite("material", &CoreData::material)
        .def_readwrite("mu", &CoreData::mu)
        .def_readwrite("al", &CoreData::al)
        .def_readwrite("ae", &CoreData::ae)
        .def_readwrite("wa", &CoreData::wa)
        .def_readwrite("le", &CoreData::le);
    py::class_<MaterialData>(m, "MaterialData")
        .def(py::init<>())
        .def_readwrite("name", &MaterialData::name)
        .def_readwrite("muOpt", &MaterialData::muOpt)
        .def_readwrite("minFrequencyHz", &MaterialData::minFrequencyHz)
        .def_readwrite("maxFrequencyHz", &MaterialData::maxFrequencyHz)
        .def_readwrite("reason", &MaterialData::reason)
        .def_readwrite("alternatives", &MaterialData::alternatives)
        .def_readwrite("bmaxT", &MaterialData::bmaxT)
        .def_readwrite("cuLossFactor", &MaterialData::cuLossFactor);
    m.def("set_core_database", &CoreDatabase::setData, "Replace the in-memory core database (called once at startup with real data)");
    m.def("set_material_database", &Materials::setData, "Replace the in-memory material database (called once at startup with real data)");

    // ============================================================
    // Phase 1 inductor design engine
    // ============================================================

    py::enum_<EvaluationStatus>(m, "EvaluationStatus")
        .value("Evaluated", EvaluationStatus::Evaluated)
        .value("NotEvaluated", EvaluationStatus::NotEvaluated)
        .value("Rejected", EvaluationStatus::Rejected);

    py::class_<DesignRules>(m, "DesignRules")
        .def(py::init<>())
        .def_readwrite("windowUtilization", &DesignRules::windowUtilization)
        .def_readwrite("allowableCurrentDensityAperCm2", &DesignRules::allowableCurrentDensityAperCm2)
        .def_readwrite("defaultFluxDensityLimitT", &DesignRules::defaultFluxDensityLimitT)
        .def_readwrite("minimumSaturationMarginPercent", &DesignRules::minimumSaturationMarginPercent)
        .def_readwrite("maximumFillFactor", &DesignRules::maximumFillFactor)
        .def_readwrite("defaultInductanceTolerancePercent", &DesignRules::defaultInductanceTolerancePercent)
        .def_readwrite("minimumSingleStrandAwg", &DesignRules::minimumSingleStrandAwg);

    m.def("design_rules_phase1_default", &DesignRules::phase1Default,
          "Return the named Phase 1 default engineering ruleset (spec section 7) - "
          "the only place Ku/Bmax/J-style constants are defined.");

    py::class_<MaterialCandidate>(m, "MaterialCandidate")
        .def(py::init<>())
        .def_readwrite("materialFamily", &MaterialCandidate::materialFamily)
        .def_readwrite("muOpt", &MaterialCandidate::muOpt)
        .def_readwrite("frequencySuitable", &MaterialCandidate::frequencySuitable)
        .def_readwrite("hasBmaxData", &MaterialCandidate::hasBmaxData)
        .def_readwrite("hasCoreLossData", &MaterialCandidate::hasCoreLossData)
        .def_readwrite("bmaxT", &MaterialCandidate::bmaxT)
        .def_readwrite("cuLossFactor", &MaterialCandidate::cuLossFactor)
        .def_readwrite("reason", &MaterialCandidate::reason)
        .def_readwrite("alternatives", &MaterialCandidate::alternatives)
        .def_readwrite("missingDataWarnings", &MaterialCandidate::missingDataWarnings);

    py::class_<CoreCandidate>(m, "CoreCandidate")
        .def(py::init<>())
        .def_readwrite("partNumber", &CoreCandidate::partNumber)
        .def_readwrite("material", &CoreCandidate::material)
        .def_readwrite("mu", &CoreCandidate::mu)
        .def_readwrite("al", &CoreCandidate::al)
        .def_readwrite("aeMm2", &CoreCandidate::aeMm2)
        .def_readwrite("waMm2", &CoreCandidate::waMm2)
        .def_readwrite("leMm", &CoreCandidate::leMm)
        .def_readwrite("areaProductCm4", &CoreCandidate::areaProductCm4)
        .def_readwrite("meetsAreaProduct", &CoreCandidate::meetsAreaProduct);

    py::class_<TurnsAndGapResult>(m, "TurnsAndGapResult")
        .def(py::init<>())
        .def_readwrite("turns", &TurnsAndGapResult::turns)
        .def_readwrite("gapMm", &TurnsAndGapResult::gapMm)
        .def_readwrite("effectiveAlNHPerTurnSquared", &TurnsAndGapResult::effectiveAlNHPerTurnSquared)
        .def_readwrite("calculatedInductanceUH", &TurnsAndGapResult::calculatedInductanceUH)
        .def_readwrite("inductanceErrorPercent", &TurnsAndGapResult::inductanceErrorPercent)
        .def_readwrite("withinTolerance", &TurnsAndGapResult::withinTolerance)
        .def_readwrite("converged", &TurnsAndGapResult::converged)
        .def_readwrite("rejectionReasons", &TurnsAndGapResult::rejectionReasons);

    py::class_<ValidationResult>(m, "ValidationResult")
        .def(py::init<>())
        .def_readwrite("passed", &ValidationResult::passed)
        .def_readwrite("checkName", &ValidationResult::checkName)
        .def_readwrite("calculatedValue", &ValidationResult::calculatedValue)
        .def_readwrite("limitValue", &ValidationResult::limitValue)
        .def_readwrite("unit", &ValidationResult::unit)
        .def_readwrite("explanation", &ValidationResult::explanation)
        .def_readwrite("usedDefaultLimit", &ValidationResult::usedDefaultLimit)
        .def_readwrite("status", &ValidationResult::status);

    py::class_<WindingDesignResult>(m, "WindingDesignResult")
        .def(py::init<>())
        .def_readwrite("wireDescription", &WindingDesignResult::wireDescription)
        .def_readwrite("conductorAreaMm2", &WindingDesignResult::conductorAreaMm2)
        .def_readwrite("parallelStrands", &WindingDesignResult::parallelStrands)
        .def_readwrite("fillFactor", &WindingDesignResult::fillFactor)
        .def_readwrite("currentDensityAperMm2", &WindingDesignResult::currentDensityAperMm2)
        .def_readwrite("fitsWindow", &WindingDesignResult::fitsWindow)
        .def_readwrite("resistanceStatus", &WindingDesignResult::resistanceStatus)
        .def_readwrite("totalWireLengthM", &WindingDesignResult::totalWireLengthM)
        .def_readwrite("dcrOhms", &WindingDesignResult::dcrOhms)
        .def_readwrite("missingData", &WindingDesignResult::missingData);

    py::class_<LossEvaluationResult>(m, "LossEvaluationResult")
        .def(py::init<>())
        .def_readwrite("copperLossStatus", &LossEvaluationResult::copperLossStatus)
        .def_readwrite("copperLossW", &LossEvaluationResult::copperLossW)
        .def_readwrite("coreLossStatus", &LossEvaluationResult::coreLossStatus)
        .def_readwrite("coreLossW", &LossEvaluationResult::coreLossW)
        .def_readwrite("highFrequencyLossStatus", &LossEvaluationResult::highFrequencyLossStatus)
        .def_readwrite("highFrequencyLossW", &LossEvaluationResult::highFrequencyLossW)
        .def_readwrite("missingData", &LossEvaluationResult::missingData);

    py::class_<ThermalEvaluationResult>(m, "ThermalEvaluationResult")
        .def(py::init<>())
        .def_readwrite("status", &ThermalEvaluationResult::status)
        .def_readwrite("predictedTempRiseC", &ThermalEvaluationResult::predictedTempRiseC)
        .def_readwrite("missingDataExplanation", &ThermalEvaluationResult::missingDataExplanation);

    py::class_<RejectionReason>(m, "RejectionReason")
        .def(py::init<>())
        .def_readwrite("checkName", &RejectionReason::checkName)
        .def_readwrite("explanation", &RejectionReason::explanation);

    py::class_<InductorCandidate>(m, "InductorCandidate")
        .def(py::init<>())
        .def_readwrite("material", &InductorCandidate::material)
        .def_readwrite("core", &InductorCandidate::core)
        .def_readwrite("turnsAndGap", &InductorCandidate::turnsAndGap)
        .def_readwrite("validations", &InductorCandidate::validations)
        .def_readwrite("winding", &InductorCandidate::winding)
        .def_readwrite("losses", &InductorCandidate::losses)
        .def_readwrite("thermal", &InductorCandidate::thermal)
        .def_readwrite("passed", &InductorCandidate::passed)
        .def_readwrite("rejectionReasons", &InductorCandidate::rejectionReasons);

    py::class_<DesignRecommendation>(m, "DesignRecommendation")
        .def(py::init<>())
        .def_readwrite("status", &DesignRecommendation::status)
        .def_readwrite("message", &DesignRecommendation::message)
        .def_readwrite("candidates", &DesignRecommendation::candidates)
        .def_readwrite("rejectedCandidates", &DesignRecommendation::rejectedCandidates)
        .def_readwrite("activeRules", &DesignRecommendation::activeRules)
        .def_readwrite("requiredAreaProductCm4", &DesignRecommendation::requiredAreaProductCm4)
        .def_readwrite("largestAvailableAreaProductCm4", &DesignRecommendation::largestAvailableAreaProductCm4);

    py::class_<InductorDesignRequest>(m, "InductorDesignRequest")
        .def(py::init<>())
        .def_readwrite("inductanceUH", &InductorDesignRequest::inductanceUH)
        .def_readwrite("peakCurrentA", &InductorDesignRequest::peakCurrentA)
        .def_readwrite("switchingFreqKHz", &InductorDesignRequest::switchingFreqKHz)
        .def_readwrite("ambientTemperatureC", &InductorDesignRequest::ambientTemperatureC)
        .def_readwrite("allowableTempRiseC", &InductorDesignRequest::allowableTempRiseC)
        .def_readwrite("rmsCurrentA", &InductorDesignRequest::rmsCurrentA)
        .def_readwrite("inductanceTolerancePercent", &InductorDesignRequest::inductanceTolerancePercent)
        .def_readwrite("averageCurrentA", &InductorDesignRequest::averageCurrentA)
        .def_readwrite("rippleCurrentPeakToPeakA", &InductorDesignRequest::rippleCurrentPeakToPeakA)
        .def_readwrite("maximumDcrMilliOhm", &InductorDesignRequest::maximumDcrMilliOhm)
        .def_readwrite("maximumWidthMm", &InductorDesignRequest::maximumWidthMm)
        .def_readwrite("maximumHeightMm", &InductorDesignRequest::maximumHeightMm)
        .def_readwrite("maximumLengthMm", &InductorDesignRequest::maximumLengthMm)
        .def_readwrite("preferredMaterialFamily", &InductorDesignRequest::preferredMaterialFamily)
        .def_readwrite("preferredCoreGeometry", &InductorDesignRequest::preferredCoreGeometry);

    m.def("run_inductor_design", &InductorDesignService::run,
          "Run the full Phase 1 inductor design pipeline and return an explainable DesignRecommendation");
}
