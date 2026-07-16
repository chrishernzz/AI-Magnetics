cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0074 NEW)

project(AIMagnetics LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# magnetics_engine/magnetics_services are static libraries linked into the
# magnetics_cpp shared module (pybind11 extension) - needs -fPIC on
# GCC/Clang or the final link fails ("recompile with -fPIC"). No effect on
# MSVC.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
execute_process(
    COMMAND ${Python3_EXECUTABLE} -m pybind11 --cmakedir
    OUTPUT_VARIABLE pybind11_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "pybind11_DIR=${pybind11_DIR}")
if(NOT pybind11_DIR STREQUAL "")
    list(APPEND CMAKE_PREFIX_PATH "${pybind11_DIR}")
endif()
find_package(pybind11 CONFIG REQUIRED PATHS "${pybind11_DIR}")



# ============================================================
# Core Engineering Engine
# ============================================================

set(CORE_SOURCES
    src/core/AreaProduct.cpp
    src/core/GapDesign.cpp
    src/core/TurnsCalculation.cpp
    src/core/CopperLoss.cpp
    src/core/CoreLoss.cpp
    src/core/HighFrequencyLosses.cpp
    src/core/MaterialEvaluation.cpp
    src/core/CoreEvaluation.cpp
    src/core/TurnsAndGapDesign.cpp
    src/core/WindingDesign.cpp
    src/core/LossEvaluation.cpp
    src/core/ThermalEvaluation.cpp
)

set(VALIDATION_SOURCES
    src/validation/DesignValidation.cpp
)

set(RULES_SOURCES
    src/rules/DesignRules.cpp
)

add_library(
    magnetics_engine
    ${CORE_SOURCES}
    ${VALIDATION_SOURCES}
    ${RULES_SOURCES}
)

target_include_directories(
    magnetics_engine
    PUBLIC
    src
)

# ============================================================
# Backend
# ============================================================

set(BACKEND_SERVICE_SOURCES
    src/backend/services/RequirementDerivationService.cpp
    src/backend/services/InductorDesignService.cpp
)

add_library(
    magnetics_services
    ${BACKEND_SERVICE_SOURCES}
)

target_link_libraries(
    magnetics_services
    PUBLIC
    magnetics_engine
)

target_include_directories(
    magnetics_services
    PUBLIC
    src
)

# ============================================================
# Python bindings
# ============================================================

pybind11_add_module(magnetics_cpp
    src/python_bindings/InductorDesignBindings.cpp
)

target_link_libraries(
    magnetics_cpp
    PRIVATE
    magnetics_services
)

target_include_directories(
    magnetics_cpp
    PRIVATE
    src
)

set_target_properties(magnetics_cpp PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/python
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/python
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/python
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/python
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/python
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/python
)


# ============================================================
# Tests
# ============================================================

enable_testing()

add_executable(magnetics_engine_tests
    tests/cpp/EngineTests.cpp
)

target_link_libraries(
    magnetics_engine_tests
    PRIVATE
    magnetics_engine
)

target_include_directories(
    magnetics_engine_tests
    PRIVATE
    src
)

add_test(NAME EngineTests COMMAND magnetics_engine_tests)

# ============================================================
# Status Messages
# ============================================================
message(STATUS "AIMagnetics configured")
message(STATUS "Python extension target: magnetics_cpp (built into python/)")
message(STATUS "Test executable: magnetics_engine_tests")
message(STATUS "Frontend folder: src/frontend")