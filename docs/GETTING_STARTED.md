# Getting Started: Build & Run AIMagnetics

This guide walks you through building the C++/pybind11 extension and running the FastAPI app locally.

---

## Prerequisites

### Windows (Recommended)
- **Visual Studio 2019+** or **Visual Studio Build Tools** (for MSVC compiler)
- **CMake 3.16+** ([download](https://cmake.org/download/))
- **Python 3.12** (pinned in `.python-version`; `pyproject.toml` requires `>=3.11`)
- **Git** (optional, for cloning)

### macOS/Linux
- **GCC 9+** or **Clang 10+**
- **CMake 3.16+**
- **Python 3.12** (pinned in `.python-version`; `pyproject.toml` requires `>=3.11`)

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
cmake --build build_pybind --config Release --target magnetics_cpp
```
This compiles `magnetics_engine` → `magnetics_services` → the `magnetics_cpp` pybind11 module, and places the compiled extension directly in `python/` (per `CMakeLists.txt`'s `LIBRARY_OUTPUT_DIRECTORY` setting), where `python/routes/inductor_design.py` imports it as `import magnetics_cpp`.

**Use `--config Release`, not `Debug`, especially on Windows.** `--config` only matters for multi-configuration generators (Visual Studio on Windows); on a single-config generator (Makefiles/Ninja, the Mac/Linux default) it's silently ignored, which is why this never showed up there. On Windows, MSVC's Debug configuration turns on full STL iterator/container checking (`_ITERATOR_DEBUG_LEVEL=2`), and this engine builds and copies a lot of `std::vector<...>` candidate lists and strings per request - under Debug that can run an order of magnitude slower than Release for no benefit (Phase 1 has no native debugger workflow that needs it). If the tool feels sluggish only on Windows, rebuild with `--config Release`.

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
pip install -r python/requirements-dev.txt
pytest tests/python
```

---

## Day-to-Day Workflow

- **Only changed Python code (routes, `app.py`)?** Just re-run Step 3 — `--reload` picks it up automatically.
- **Changed any C++ code or the pybind11 binding file?** Re-run Step 2 before Step 3.

See [WHEN_TO_RUN_PROGRAM.md](WHEN_TO_RUN_PROGRAM.md) for the exact commands.

---

## Deploying to Vercel

The app deploys to Vercel only (no separate frontend host) — `python/app.py`
serves the frontend *and* answers `POST /inductor-design` from one process,
so there's a single URL to share.

**`vercel.json`** routes every request to `python/app.py` via `@vercel/python`.

**Python version is pinned to 3.12** (`.python-version` at the repo root
and inside `python/`) — this has to match exactly, because the compiled
C++ extension is ABI-specific to a Python minor version.

**The compiled extension is checked into git**, not built by Vercel:
`python/magnetics_cpp.cpython-312-x86_64-linux-gnu.so`. Vercel's Python
builder has no CMake build step, so there's nothing that would compile
`magnetics_cpp` during deployment - the prebuilt binary is the only
reason the import in `python/app.py`/`python/routes/inductor_design.py`
succeeds there. It's a `.gitignore` exception (`python/magnetics_cpp*.so`
is otherwise ignored).

**If you ever need to rebuild that binary** (engine code changed, or the
checked-in one stops working), it must be compiled with **g++-11
specifically, not a newer GCC** - GCC 13 (the default on newer systems)
produces a binary that needs a GLIBCXX version newer than what Vercel's
runtime (Amazon Linux 2023) ships, which fails to import there even
though it works fine locally. Compile against Python 3.12 to match the
pin:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DPython3_EXECUTABLE=/path/to/python3.12 \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++-11
cmake --build build --target magnetics_cpp
```

Then copy the resulting `magnetics_cpp.cpython-312-x86_64-linux-gnu.so`
into `python/` and commit it.

**This has already bitten the project once**: the v1-optimization merge
(real core loss + loss-based candidate ranking) and a later frontend-only
commit both landed on `main` without anyone rebuilding this binary. The
live Vercel site kept serving an older `.so` — real copper-loss numbers
(from an even earlier change) but candidates still ordered by area product
instead of total loss, with a frontend that visually claimed loss-based
ranking. Nothing failed loudly; the two layers just quietly disagreed.
**Any change under `src/` (the C++ engine) is not actually live on Vercel
until this binary is rebuilt and committed separately** - a source commit
alone is not enough. Frontend-only or Python-route-only changes don't need
a rebuild.

---

## Note

There is no standalone `magnetics_server` executable — the FastAPI app (`python/app.py`) is the only entry point. If you see an older instruction referencing `magnetics_server.exe` or a port-8080 C++ server, that describes an earlier plan that's no longer how the project is built.