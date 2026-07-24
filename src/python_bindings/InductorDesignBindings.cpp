#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "backend/services/InductorDesignService.h"
#include "backend/services/BuckElectricalSolver.h"
#include "core/model/TopologyInput.h"
#include "core/model/ConductionMode.h"
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
#include "core/winding/WindingConstructionType.h"
#include "core/losses/CoreLoss.h"
#include "core/magnetics/GapMethod.h"
#include "core/model/Provenance.h"
#include "core/model/HardwareValidation.h"
#include "core/model/EngineVersions.h"
#include "data/CoreDatabase.h"
#include "data/CoreLossCoefficientDatabase.h"
#include "data/MaterialDatabase.h"
#include "rules/DesignRules.h"
#include "validation/EvaluationStatus.h"
#include "validation/Validation.h"
#include "validation/DesignValidation.h"

namespace py = pybind11;

PYBIND11_MODULE(magnetics_cpp, m) {
    m.doc() = "AIMagnetics C++ bindings for Python";

    /*============================================================
    Area product / stored energy - shared by the Phase 1 pipeline
    (called internally in C++ by InductorDesignService) and by
    tests/python/test_unit_conversions.py, which calls these directly
    to verify the formula independently of the full pipeline.
    ============================================================*/
    py::class_<AreaProductInput>(m, "AreaProductInput")
        .def(py::init<>())
        .def_readwrite("inductanceH", &AreaProductInput::inductanceH)
        .def_readwrite("peakCurrentA", &AreaProductInput::peakCurrentA)
        .def_readwrite("switchingFreqHz", &AreaProductInput::switchingFreqHz)
        .def_readwrite("allowableTempRiseC", &AreaProductInput::allowableTempRiseC)
        .def_readwrite("windowUtilization", &AreaProductInput::windowUtilization)
        .def_readwrite("fluxDensityT", &AreaProductInput::fluxDensityT)
        .def_readwrite("currentDensityAPerCm2", &AreaProductInput::currentDensityAPerCm2);

    //areaproductinput contains valid nonzero inductance and current values : returns required area product in cm^4 for magnetic core selection
    m.def("calculate_ap", &calculateAp, "Calculate the area product (Ap) from input");
    //inductance and current are provided in henries and amperes : returns stored magnetic energy in joules
    m.def("calculate_stored_energy", &calculateStoredEnergy, "Calculate stored energy for the given inductance and current");

    //Raw data structs, populated once at FastAPI startup with real data (see python/app.py) - both Materials and Cores are loaded here.
    py::class_<CoreData>(m, "CoreData")
        .def(py::init<>())
        .def_readwrite("partNumber", &CoreData::partNumber)
        .def_readwrite("material", &CoreData::material)
        .def_readwrite("mu", &CoreData::mu)
        .def_readwrite("al", &CoreData::al)
        .def_readwrite("ae", &CoreData::ae)
        .def_readwrite("wa", &CoreData::wa)
        .def_readwrite("le", &CoreData::le)
        .def_readwrite("mlt", &CoreData::mlt)
        .def_readwrite("vendor", &CoreData::vendor);
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
    py::class_<CoreLossCoefficientData>(m, "CoreLossCoefficientData")
        .def(py::init<>())
        .def_readwrite("materialName", &CoreLossCoefficientData::materialName)
        .def_readwrite("minFreqHz", &CoreLossCoefficientData::minFreqHz)
        .def_readwrite("maxFreqHz", &CoreLossCoefficientData::maxFreqHz)
        .def_readwrite("k", &CoreLossCoefficientData::k)
        .def_readwrite("alpha", &CoreLossCoefficientData::alpha)
        .def_readwrite("beta", &CoreLossCoefficientData::beta)
        .def_readwrite("ct0", &CoreLossCoefficientData::ct0)
        .def_readwrite("ct1", &CoreLossCoefficientData::ct1)
        .def_readwrite("ct2", &CoreLossCoefficientData::ct2)
        .def_readwrite("minFluxSwingT", &CoreLossCoefficientData::minFluxSwingT)
        .def_readwrite("maxFluxSwingT", &CoreLossCoefficientData::maxFluxSwingT)
        .def_readwrite("testTemperatureC", &CoreLossCoefficientData::testTemperatureC);

    //vector contains valid core records : in-memory core database is replaced with supplied data
    m.def("set_core_database", &CoreDatabase::setData, "Replace the in-memory core database (called once at startup with real data)");
    //vector contains valid material records : in-memory material database is replaced with supplied data
    m.def("set_material_database", &MaterialDatabase::setData, "Replace the in-memory material database (called once at startup with real data)");
    //coefficient records are available for supported materials : in-memory loss coefficient database is updated
    m.def("set_core_loss_coefficient_database", &CoreLossCoefficientDatabase::setData, "Replace the in-memory core-loss (Steinmetz) coefficient database (called once at startup with real data)");

    py::class_<CoreLossCoefficientLookup>(m, "CoreLossCoefficientLookup")
        .def(py::init<>())
        .def_readwrite("found", &CoreLossCoefficientLookup::found)
        .def_readwrite("coefficients", &CoreLossCoefficientLookup::coefficients);
    m.def("find_core_loss_coefficients", &findCoreLossCoefficients, "Look up the real Steinmetz coefficient row for a material at a given switching frequency, if one exists");

    /*============================================================
    Phase 1 inductor design engine
    ============================================================*/
    py::enum_<EvaluationStatus>(m, "EvaluationStatus")
        .value("Evaluated", EvaluationStatus::Evaluated)
        .value("NotEvaluated", EvaluationStatus::NotEvaluated)
        .value("Rejected", EvaluationStatus::Rejected);

    py::enum_<GapMethod>(m, "GapMethod")
        .value("MachinedCenterLeg", GapMethod::MachinedCenterLeg)
        .value("Spacer", GapMethod::Spacer)
        .value("Distributed", GapMethod::Distributed)
        .value("ManufacturerGapped", GapMethod::ManufacturerGapped);

    py::enum_<WindingConstructionType>(m, "WindingConstructionType")
        .value("SingleRoundWire", WindingConstructionType::SingleRoundWire)
        .value("ParallelRoundWires", WindingConstructionType::ParallelRoundWires)
        .value("Foil", WindingConstructionType::Foil)
        .value("Busbar", WindingConstructionType::Busbar)
        .value("PCB", WindingConstructionType::PCB);

    py::class_<DesignRules>(m, "DesignRules")
        .def(py::init<>())
        .def_readwrite("windowUtilization", &DesignRules::windowUtilization)
        .def_readwrite("allowableCurrentDensityAperCm2", &DesignRules::allowableCurrentDensityAperCm2)
        .def_readwrite("defaultFluxDensityLimitT", &DesignRules::defaultFluxDensityLimitT)
        .def_readwrite("minimumSaturationMarginPercent", &DesignRules::minimumSaturationMarginPercent)
        .def_readwrite("maximumFillFactor", &DesignRules::maximumFillFactor)
        .def_readwrite("defaultInductanceTolerancePercent", &DesignRules::defaultInductanceTolerancePercent)
        .def_readwrite("minimumSingleStrandAwg", &DesignRules::minimumSingleStrandAwg)
        .def_readwrite("maximumRippleCurrentPercent", &DesignRules::maximumRippleCurrentPercent)
        .def_readwrite("recommendedFluxDerateFactor", &DesignRules::recommendedFluxDerateFactor)
        .def_readwrite("minManufacturableGapMm", &DesignRules::minManufacturableGapMm)
        .def_readwrite("gapStepMm", &DesignRules::gapStepMm)
        .def_readwrite("maxGapFraction", &DesignRules::maxGapFraction)
        .def_readwrite("gapTolerancePercent", &DesignRules::gapTolerancePercent)
        .def_readwrite("gapMethod", &DesignRules::gapMethod)
        .def_readwrite("singleBuildInsulationBuildUpMm", &DesignRules::singleBuildInsulationBuildUpMm)
        .def_readwrite("packingFactor", &DesignRules::packingFactor)
        .def_readwrite("bobbinWindowDerateFactor", &DesignRules::bobbinWindowDerateFactor)
        .def_readwrite("marginAllowanceAreaFraction", &DesignRules::marginAllowanceAreaFraction)
        .def_readwrite("leadExitAllowanceAreaFraction", &DesignRules::leadExitAllowanceAreaFraction)
        .def_readwrite("currentSharingDerateFactor", &DesignRules::currentSharingDerateFactor)
        .def_readwrite("totalLeadLengthAllowanceMm", &DesignRules::totalLeadLengthAllowanceMm)
        .def_readwrite("routingLengthAllowanceMm", &DesignRules::routingLengthAllowanceMm)
        .def_readwrite("connectionResistanceMilliOhm", &DesignRules::connectionResistanceMilliOhm)
        .def_readwrite("copperTempCoefficientPerC", &DesignRules::copperTempCoefficientPerC)
        .def_readwrite("assumedWindingTempCWhenThermalNotEvaluated", &DesignRules::assumedWindingTempCWhenThermalNotEvaluated)
        .def_readwrite("defaultThermalResistanceCPerW", &DesignRules::defaultThermalResistanceCPerW)
        .def_readwrite("thermalConvergenceThresholdC", &DesignRules::thermalConvergenceThresholdC)
        .def_readwrite("maxThermalIterations", &DesignRules::maxThermalIterations)
        .def_readwrite("skinDepthRiskModerateThreshold", &DesignRules::skinDepthRiskModerateThreshold)
        .def_readwrite("skinDepthRiskHighThreshold", &DesignRules::skinDepthRiskHighThreshold);

    m.def("design_rules_phase1_default", &DesignRules::phase1Default,
          "Return the named Phase 1 default engineering ruleset (spec section 7) - "
          "the only place Ku/Bmax/J-style constants are defined.");

    py::enum_<DataConfidence>(m, "DataConfidence")
        .value("Manufacturer", DataConfidence::Manufacturer)
        .value("Measured", DataConfidence::Measured)
        .value("Derived", DataConfidence::Derived)
        .value("Estimated", DataConfidence::Estimated)
        .value("Phase1Default", DataConfidence::Phase1Default);

    py::class_<SourceInfo>(m, "SourceInfo")
        .def(py::init<>())
        .def_readwrite("manufacturer", &SourceInfo::manufacturer)
        .def_readwrite("partNumber", &SourceInfo::partNumber)
        .def_readwrite("materialGrade", &SourceInfo::materialGrade)
        .def_readwrite("datasheetName", &SourceInfo::datasheetName)
        .def_readwrite("datasheetRevision", &SourceInfo::datasheetRevision)
        .def_readwrite("datasheetUrl", &SourceInfo::datasheetUrl)
        .def_readwrite("dateAccessed", &SourceInfo::dateAccessed)
        .def_readwrite("confidence", &SourceInfo::confidence)
        .def_readwrite("confidenceNote", &SourceInfo::confidenceNote);

    py::class_<HardwareValidationRecord>(m, "HardwareValidationRecord")
        .def(py::init<>())
        .def_readwrite("measuredInductanceUH", &HardwareValidationRecord::measuredInductanceUH)
        .def_readwrite("measuredDcrMilliOhm", &HardwareValidationRecord::measuredDcrMilliOhm)
        .def_readwrite("measuredRippleCurrentA", &HardwareValidationRecord::measuredRippleCurrentA)
        .def_readwrite("measuredPeakCurrentA", &HardwareValidationRecord::measuredPeakCurrentA)
        .def_readwrite("measuredCoreTempC", &HardwareValidationRecord::measuredCoreTempC)
        .def_readwrite("measuredWindingTempC", &HardwareValidationRecord::measuredWindingTempC)
        .def_readwrite("predictedVsMeasuredInductanceErrorPercent", &HardwareValidationRecord::predictedVsMeasuredInductanceErrorPercent)
        .def_readwrite("predictedVsMeasuredDcrErrorPercent", &HardwareValidationRecord::predictedVsMeasuredDcrErrorPercent)
        .def_readwrite("testNotes", &HardwareValidationRecord::testNotes)
        .def_readwrite("testDate", &HardwareValidationRecord::testDate)
        .def_readwrite("status", &HardwareValidationRecord::status);

    py::class_<EngineVersions>(m, "EngineVersions")
        .def(py::init<>())
        .def_readwrite("calculationEngineVersion", &EngineVersions::calculationEngineVersion)
        .def_readwrite("designRulesVersion", &EngineVersions::designRulesVersion)
        .def_readwrite("coreDatabaseVersion", &EngineVersions::coreDatabaseVersion)
        .def_readwrite("materialDatabaseVersion", &EngineVersions::materialDatabaseVersion);

    m.def("set_engine_versions", &EngineVersionsStore::setDatabaseVersions,
          "Set the core/material database version strings (real content hashes of the loaded CSVs) - "
          "called once at FastAPI startup, read by every run's DesignRecommendation.versions afterward.");

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
        .def_readwrite("missingDataWarnings", &MaterialCandidate::missingDataWarnings)
        .def_readwrite("source", &MaterialCandidate::source);

    py::class_<CoreCandidate>(m, "CoreCandidate")
        .def(py::init<>())
        .def_readwrite("partNumber", &CoreCandidate::partNumber)
        .def_readwrite("material", &CoreCandidate::material)
        .def_readwrite("mu", &CoreCandidate::mu)
        .def_readwrite("al", &CoreCandidate::al)
        .def_readwrite("aeMm2", &CoreCandidate::aeMm2)
        .def_readwrite("waMm2", &CoreCandidate::waMm2)
        .def_readwrite("leMm", &CoreCandidate::leMm)
        .def_readwrite("mltMm", &CoreCandidate::mltMm)
        .def_readwrite("areaProductCm4", &CoreCandidate::areaProductCm4)
        .def_readwrite("meetsAreaProduct", &CoreCandidate::meetsAreaProduct)
        .def_readwrite("vendor", &CoreCandidate::vendor)
        .def_readwrite("source", &CoreCandidate::source);

    py::class_<TurnsAndGapResult>(m, "TurnsAndGapResult")
        .def(py::init<>())
        .def_readwrite("turns", &TurnsAndGapResult::turns)
        .def_readwrite("gapMm", &TurnsAndGapResult::gapMm)
        .def_readwrite("effectiveAlNHPerTurnSquared", &TurnsAndGapResult::effectiveAlNHPerTurnSquared)
        .def_readwrite("calculatedInductanceUH", &TurnsAndGapResult::calculatedInductanceUH)
        .def_readwrite("inductanceErrorPercent", &TurnsAndGapResult::inductanceErrorPercent)
        .def_readwrite("withinTolerance", &TurnsAndGapResult::withinTolerance)
        .def_readwrite("converged", &TurnsAndGapResult::converged)
        .def_readwrite("rejectionReasons", &TurnsAndGapResult::rejectionReasons)
        .def_readwrite("gapMethod", &TurnsAndGapResult::gapMethod)
        .def_readwrite("gapMinMm", &TurnsAndGapResult::gapMinMm)
        .def_readwrite("gapMaxMm", &TurnsAndGapResult::gapMaxMm)
        .def_readwrite("inductanceAtMinGapUH", &TurnsAndGapResult::inductanceAtMinGapUH)
        .def_readwrite("inductanceAtMaxGapUH", &TurnsAndGapResult::inductanceAtMaxGapUH)
        .def_readwrite("inductanceWithinToleranceAcrossGapRange", &TurnsAndGapResult::inductanceWithinToleranceAcrossGapRange)
        .def_readwrite("smallGapWarning", &TurnsAndGapResult::smallGapWarning)
        .def_readwrite("smallGapWarningReason", &TurnsAndGapResult::smallGapWarningReason);

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

    py::class_<FluxLimitTiers>(m, "FluxLimitTiers")
        .def(py::init<>())
        .def_readwrite("absoluteSaturationT", &FluxLimitTiers::absoluteSaturationT)
        .def_readwrite("absoluteSaturationIsDefault", &FluxLimitTiers::absoluteSaturationIsDefault)
        .def_readwrite("recommendedOperatingT", &FluxLimitTiers::recommendedOperatingT)
        .def_readwrite("temperatureAdjustedStatus", &FluxLimitTiers::temperatureAdjustedStatus)
        .def_readwrite("temperatureAdjustedExplanation", &FluxLimitTiers::temperatureAdjustedExplanation)
        .def_readwrite("coreLossLimitedStatus", &FluxLimitTiers::coreLossLimitedStatus)
        .def_readwrite("coreLossLimitedExplanation", &FluxLimitTiers::coreLossLimitedExplanation);
    m.def("calculate_flux_limit_tiers", &calculateFluxLimitTiers, "Compute the informational flux-limit tier breakdown for a material candidate");

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
        .def_readwrite("missingData", &WindingDesignResult::missingData)
        .def_readwrite("constructionType", &WindingDesignResult::constructionType)
        .def_readwrite("insulatedConductorDiameterMm", &WindingDesignResult::insulatedConductorDiameterMm)
        .def_readwrite("insulatedConductorAreaMm2", &WindingDesignResult::insulatedConductorAreaMm2)
        .def_readwrite("physicalDescription", &WindingDesignResult::physicalDescription)
        .def_readwrite("physicalWindowAreaMm2", &WindingDesignResult::physicalWindowAreaMm2)
        .def_readwrite("physicalWindowFillFactor", &WindingDesignResult::physicalWindowFillFactor)
        .def_readwrite("fitsPhysicalWindow", &WindingDesignResult::fitsPhysicalWindow)
        .def_readwrite("effectiveCurrentDensityAperMm2", &WindingDesignResult::effectiveCurrentDensityAperMm2)
        .def_readwrite("bundleFitStatus", &WindingDesignResult::bundleFitStatus)
        .def_readwrite("coreWindingLengthM", &WindingDesignResult::coreWindingLengthM)
        .def_readwrite("leadLengthM", &WindingDesignResult::leadLengthM)
        .def_readwrite("routingLengthM", &WindingDesignResult::routingLengthM)
        .def_readwrite("totalLengthM", &WindingDesignResult::totalLengthM)
        .def_readwrite("connectionResistanceOhms", &WindingDesignResult::connectionResistanceOhms)
        .def_readwrite("coldDcrOhmsAt20C", &WindingDesignResult::coldDcrOhmsAt20C)
        .def_readwrite("estimatedHotDcrOhms", &WindingDesignResult::estimatedHotDcrOhms);

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
        .def_readwrite("rejectionReasons", &InductorCandidate::rejectionReasons)
        .def_readwrite("hardwareValidation", &InductorCandidate::hardwareValidation)
        .def_readwrite("fluxLimits", &InductorCandidate::fluxLimits);

    py::class_<DesignRecommendation>(m, "DesignRecommendation")
        .def(py::init<>())
        .def_readwrite("status", &DesignRecommendation::status)
        .def_readwrite("message", &DesignRecommendation::message)
        .def_readwrite("candidates", &DesignRecommendation::candidates)
        .def_readwrite("rejectedCandidates", &DesignRecommendation::rejectedCandidates)
        .def_readwrite("activeRules", &DesignRecommendation::activeRules)
        .def_readwrite("requiredAreaProductCm4", &DesignRecommendation::requiredAreaProductCm4)
        .def_readwrite("largestAvailableAreaProductCm4", &DesignRecommendation::largestAvailableAreaProductCm4)
        .def_readwrite("versions", &DesignRecommendation::versions);

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

    /*
    MODE 1: Topology is not exposed here - TopologyInput's constructor
    already defaults it to Topology::Buck, the only implemented value, so
    there is nothing for Python to set. Adding Boost/Flyback later is the
    point where this would need its own py::enum_.
    */
    py::class_<TopologyInput>(m, "TopologyInput")
        .def(py::init<>())
        .def_readwrite("vinMinV", &TopologyInput::vinMinV)
        .def_readwrite("vinMaxV", &TopologyInput::vinMaxV)
        .def_readwrite("voutV", &TopologyInput::voutV)
        .def_readwrite("ioutA", &TopologyInput::ioutA)
        .def_readwrite("switchingFreqKHz", &TopologyInput::switchingFreqKHz)
        .def_readwrite("rippleCurrentPercent", &TopologyInput::rippleCurrentPercent)
        .def_readwrite("ambientTemperatureC", &TopologyInput::ambientTemperatureC)
        .def_readwrite("allowableTempRiseC", &TopologyInput::allowableTempRiseC)
        .def_readwrite("inductanceTolerancePercent", &TopologyInput::inductanceTolerancePercent);

    py::enum_<ConductionMode>(m, "ConductionMode")
        .value("CCM", ConductionMode::CCM)
        .value("CCMBoundary", ConductionMode::CCMBoundary)
        .value("DCMUnsupported", ConductionMode::DCMUnsupported);

    py::class_<BuckOperatingPointResult>(m, "BuckOperatingPointResult")
        .def(py::init<>())
        .def_readwrite("vinV", &BuckOperatingPointResult::vinV)
        .def_readwrite("dutyCycle", &BuckOperatingPointResult::dutyCycle)
        .def_readwrite("rippleCurrentPeakToPeakA", &BuckOperatingPointResult::rippleCurrentPeakToPeakA)
        .def_readwrite("peakCurrentA", &BuckOperatingPointResult::peakCurrentA)
        .def_readwrite("minInductorCurrentA", &BuckOperatingPointResult::minInductorCurrentA);

    py::class_<BuckSolveResult>(m, "BuckSolveResult")
        .def(py::init<>())
        .def_readwrite("request", &BuckSolveResult::request)
        .def_readwrite("atVinMin", &BuckSolveResult::atVinMin)
        .def_readwrite("atVinMax", &BuckSolveResult::atVinMax)
        .def_readwrite("worstCaseDutyCycleAt", &BuckSolveResult::worstCaseDutyCycleAt)
        .def_readwrite("worstCaseRippleAt", &BuckSolveResult::worstCaseRippleAt)
        .def_readwrite("worstCasePeakAt", &BuckSolveResult::worstCasePeakAt)
        .def_readwrite("conductionMode", &BuckSolveResult::conductionMode)
        .def_readwrite("assumptions", &BuckSolveResult::assumptions)
        .def_readwrite("warnings", &BuckSolveResult::warnings);

    m.def("solve_buck_topology", &BuckElectricalSolver::solve,
          "MODE 1: derive a BuckSolveResult (sized InductorDesignRequest, real "
          "electrical quantities at both Vin_min and Vin_max, CCM/DCM classification, "
          "and stated Phase 1 assumptions) from Buck converter operating requirements");
}
