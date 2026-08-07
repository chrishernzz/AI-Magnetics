cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0074 NEW)

project(AIMagnetics LANGUAGES CXX)

# Single-config generators (Makefiles/Ninja - the Mac/Linux default) build
# with no optimization at all if CMAKE_BUILD_TYPE is unset, which is silent
# and easy to not notice. Multi-config generators (Visual Studio) pick their
# config via --config at build time instead (see docs/GETTING_STARTED.md -
# use --config Release, not Debug: MSVC's Debug STL iterator/container
# checking makes this candidate-list-heavy engine dramatically slower for
# no benefit here), so this only applies to the single-config case.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

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
    src/core/sizing/AreaProduct.cpp
    src/core/sizing/MaterialEvaluation.cpp
    src/core/sizing/CoreEvaluation.cpp
    src/core/sizing/FluxLimit.cpp
    src/core/magnetics/GapDesign.cpp
    src/core/magnetics/PermeabilityRolloff.cpp
    src/core/magnetics/TurnsCalculation.cpp
    src/core/magnetics/TurnsAndGapDesign.cpp
    src/core/winding/WindingDesign.cpp
    src/core/losses/CopperLoss.cpp
    src/core/losses/CoreLoss.cpp
    src/core/losses/SkinDepthRisk.cpp
    src/core/losses/LossEvaluation.cpp
    src/core/thermal/ThermalEvaluation.cpp
)

set(VALIDATION_SOURCES
    src/validation/DesignValidation.cpp
    src/validation/RecommendationStatus.cpp
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
    src/backend/services/BuckElectricalSolver.cpp
    src/backend/services/CandidateRankingHelpers.cpp
    src/backend/services/RankingHighlightsService.cpp
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
    tests/cpp/GapDesignTests.cpp
    tests/cpp/UnitConversionTests.cpp
    tests/cpp/ValidationTests.cpp
    tests/cpp/AreaProductTests.cpp
    tests/cpp/CoreEvaluationTests.cpp
    tests/cpp/WindingDesignTests.cpp
    tests/cpp/LossTests.cpp
    tests/cpp/GapToleranceTests.cpp
    tests/cpp/PermeabilityRolloffTests.cpp
    tests/cpp/ThermalTests.cpp
    tests/cpp/RequirementDerivationServiceTests.cpp
    tests/cpp/RankingHighlightsTests.cpp
)

target_link_libraries(
    magnetics_engine_tests
    PRIVATE
    magnetics_engine
    magnetics_services
)

target_include_directories(
    magnetics_engine_tests
    PRIVATE
    src
)

# The default build type is Release (see the comment above), which defines
# NDEBUG and makes every assert() in tests/cpp/*.cpp a silent no-op - the
# test binary would still build, run, and print "All ... passed" even if
# every single assertion in it was false. -UNDEBUG here re-enables assert()
# for this target specifically, without giving up Release optimization for
# the engine libraries the comment above explains the default is for.
target_compile_options(magnetics_engine_tests PRIVATE -UNDEBUG)

add_test(NAME EngineTests COMMAND magnetics_engine_tests)

# ============================================================
# Status Messages
# ============================================================
message(STATUS "AIMagnetics configured")
message(STATUS "Python extension target: magnetics_cpp (built into python/)")
message(STATUS "Test executable: magnetics_engine_tests")
message(STATUS "Frontend folder: frontend")