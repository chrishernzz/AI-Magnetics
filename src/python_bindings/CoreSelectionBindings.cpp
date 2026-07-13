#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "../backend/services/CoreSelectionService.h"
#include "../backend/services/MaterialSelectionService.h"
#include "../core/CoreSelection.h"
#include "../core/MaterialSelection.h"
#include "../core/AreaProduct.h"

namespace py = pybind11;

PYBIND11_MODULE(magnetics_cpp, m) {
    m.doc() = "AIMagnetics C++ bindings for Python";

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
}