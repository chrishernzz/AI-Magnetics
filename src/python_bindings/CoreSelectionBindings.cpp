#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "../backend/services/CoreSelectionService.h"
#include "../backend/services/MaterialSelectionService.h"
#include "../core/CoreSelection.h"
#include "../core/MaterialSelection.h"
#include "../core/AreaProduct.h"
#include "../core/TurnsCalculation.h"
#include "../data/CoreDatabase.h"
#include "../data/Materials.h"

namespace py = pybind11;

PYBIND11_MODULE(magnetics_cpp, m) {
    m.doc() = "AIMagnetics C++ bindings for Python";

    // Core Selection 
    py::class_<CoreSelectionInput>(m, "CoreSelectionInput")
        .def(py::init<>())
        .def_readwrite("areaProduct", &CoreSelectionInput::areaProduct)
        .def_readwrite("peakCurrentA", &CoreSelectionInput::peakCurrentA)
        .def_readwrite("recommendedMaterial", &CoreSelectionInput::recommendedMaterial);
    py::class_<CoreSelectionResult>(m, "CoreSelectionResult")
        .def(py::init<>())
        .def_readwrite("partNumber", &CoreSelectionResult::partNumber)
        .def_readwrite("material", &CoreSelectionResult::material)
        .def_readwrite("mu", &CoreSelectionResult::mu)
        .def_readwrite("al", &CoreSelectionResult::al)
        .def_readwrite("ae", &CoreSelectionResult::ae)
        .def_readwrite("wa", &CoreSelectionResult::wa)
        .def_readwrite("le", &CoreSelectionResult::le);
    py::class_<CoreSelectionService>(m, "CoreSelectionService")
        .def(py::init<>())
        .def("calculate", &CoreSelectionService::calculate);

    //Material Selection
    py::class_<MaterialSelectionInput>(m, "MaterialSelectionInput")
        .def(py::init<>())
        .def_readwrite("inductanceH", &MaterialSelectionInput::inductanceH)
        .def_readwrite("peakCurrentA", &MaterialSelectionInput::peakCurrentA)
        .def_readwrite("switchingFreqHz", &MaterialSelectionInput::switchingFreqHz)
        .def_readwrite("allowableTempRiseC", &MaterialSelectionInput::allowableTempRiseC)
        .def_readwrite("waveformFactor", &MaterialSelectionInput::waveformFactor);
    py::class_<MaterialSelectionResult>(m, "MaterialSelectionResult")
        .def(py::init<>())
        .def_readwrite("materialFamily", &MaterialSelectionResult::materialFamily)
        .def_readwrite("muOpt", &MaterialSelectionResult::muOpt)
        .def_readwrite("reason", &MaterialSelectionResult::reason)
        .def_readwrite("alternatives", &MaterialSelectionResult::alternatives);
    py::class_<MaterialSelectionService>(m, "MaterialSelectionService")
        .def(py::init<>())
        .def("calculate", &MaterialSelectionService::calculate);

    //Area Product Calculation
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

    m.def("select_core", &selectCore, "Select core by input");

    //Turns Calculation
     py::class_<TurnsCalculationInput>(m, "TurnsCalculationInput")
        .def(py::init<>())
        .def_readwrite("inductanceUH", &TurnsCalculationInput::inductanceUH)
        .def_readwrite("core", &TurnsCalculationInput::core);
    py::class_<TurnsCalculationResult>(m, "TurnsCalculationResult")
        .def(py::init<>())
        .def_readwrite("turns", &TurnsCalculationResult::turns)
        .def_readwrite("inductanceUH", &TurnsCalculationResult::inductanceUH)
        .def_readwrite("al", &TurnsCalculationResult::al);\
    m.def("calculate_turns", &calculateTurns, "Calculate turns");

    /*expose the raw data structs so Python can build real records (from PyOpenMagnetics)
    and hand them down to the C++ Engine, instead of C++ reading cores.csv/materials.csv itelslf*/
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
        .def_readwrite("alternatives", &MaterialData::alternatives);
    //called once at FastAPI startup with real data (see python/app.py)
    m.def("set_core_database", &CoreDatabase::setData,"Replace the in-memory core database (called once at startup with real data)");
    m.def("set_material_database", &Materials::setData,"Replace the in-memory material database (called once at startup with real data)");

}