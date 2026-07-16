# Getting Started: Build & Run AIMagnetics

This guide walks you through building the C++/pybind11 extension and running the FastAPI app locally.

---

## Prerequisites

### Windows (Recommended)
- **Visual Studio 2019+** or **Visual Studio Build Tools** (for MSVC compiler)
- **CMake 3.16+** ([download](https://cmake.org/download/))
- **Python 3.9+**
- **Git** (optional, for cloning)

### macOS/Linux
- **GCC 9+** or **Clang 10+**
- **CMake 3.16+**
- **Python 3.9+**

### Python packages
```bash
pip install pybind11
pip install -r python/requirements.txt # fastapi, uvicorn[standard]
```
`pybind11` must be installed *before* running CMake — `CMakeLists.txt` calls `python -m pybind11 --cmakedir` to locate it.

The real core/material data (`data/real_materials.csv`, `data/real_cores.csv`)
is already checked into the repo — no extra install needed to run the app.
`PyOpenMagnetics` is only needed if you're regenerating that data via
`scripts/export_real_data.py`, and only runs on Linux/macOS/WSL2 — see
`docs/ARCHITECTURE.md` → "Data Source" for why.

---

## Build Instructions

### Step 1: Configure with CMake
```bash
# From the project root
cmake -S . -B build_pybind
```

### Step 2: Build the Python extension
```bash
cmake --build build_pybind --config Debug --target magnetics_cpp
```
This compiles `magnetics_engine` → `magnetics_services` → the `magnetics_cpp` pybind11 module, and places the compiled extension directly in `python/` (per `CMakeLists.txt`'s `LIBRARY_OUTPUT_DIRECTORY` setting), where `python/routes/inductor_design.py` imports it as `import magnetics_cpp`.

### Step 3: Run the app
```bash
python -m uvicorn python.app:app --reload --host 127.0.0.1 --port 8000
```

### Step 4: Open the UI
Navigate to **http://127.0.0.1:8000** in a browser.

---

## Running Tests

```bash
# C++ unit-conversion and gap/AL formula checks
cmake --build build_pybind --target magnetics_engine_tests
ctest --test-dir build_pybind -C Debug -V

# Python: scenario/reference-design checks against data/test_scenarios.csv
# and data/reference_designs.csv (needs magnetics_cpp built first - Step 2 above)
pip install -r python/requirements-dev.tno it xt
pytest tests/python
```

---

## Day-to-Day Workflow

- **Only changed Python code (routes, `app.py`)?** Just re-run Step 3 — `--reload` picks it up automatically.
- **Changed any C++ code or the pybind11 binding file?** Re-run Step 2 before Step 3.

See [WHEN_TO_RUN_PROGRAM.md](WHEN_TO_RUN_PROGRAM.md) for the exact commands.

---

## Note

There is no standalone `magnetics_server` executable — the FastAPI app (`python/app.py`) is the only entry point. If you see an older instruction referencing `magnetics_server.exe` or a port-8080 C++ server, that describes an earlier plan that's no longer how the project is built.