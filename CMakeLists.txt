cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0074 NEW)

project(AIMagnetics LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    src/core/MaterialSelection.cpp
    src/core/AreaProduct.cpp
    src/core/CoreSelection.cpp
    src/core/GapDesign.cpp
    src/core/TurnsCalculation.cpp
    src/core/CopperLoss.cpp
    src/core/CoreLoss.cpp
    src/core/HighFrequencyLosses.cpp
)

set(VALIDATION_SOURCES
)

set(RULES_SOURCES
)

set(DATA_SOURCES
    src/data/Materials.cpp
    src/data/CoreDatabase.cpp
)

add_library(
    magnetics_engine
    ${CORE_SOURCES}
    ${VALIDATION_SOURCES}
    ${RULES_SOURCES}
    ${DATA_SOURCES}
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
    src/backend/services/AreaProductService.cpp
    src/backend/services/MaterialSelectionService.cpp
    src/backend/services/CoreSelectionService.cpp
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
    src/python_bindings/CoreSelectionBindings.cpp
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
# Status Messages
# ============================================================
message(STATUS "AIMagnetics configured")
message(STATUS "Server executable: magnetics_server")
message(STATUS "Frontend folder: src/frontend")