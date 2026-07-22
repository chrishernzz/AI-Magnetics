# Getting Started: Build & Run AIMagnetics

This guide walks you through building the C++/pybind11 extension and running the FastAPI app locally.

---

## Prerequisites

You need a C++ compiler, CMake, and Python 3.11+ (3.12 recommended — see the
version-matching warning below). None of this is optional: the design engine
is C++ compiled into a Python extension (`magnetics_cpp`), not pure Python.

### Windows
- **A C++ compiler**: install **"Build Tools for Visual Studio"** ([download](https://visualstudio.microsoft.com/downloads/) → scroll to "Tools for Visual Studio") with the **"Desktop development with C++"** workload checked. A plain VS Code install does *not* include this — CMake's configure step fails with "no CMAKE_CXX_COMPILER could be found" without it.
- **CMake 3.16+** — easiest via `pip install cmake` (see the PATH note below), or the [official installer](https://cmake.org/download/).
- **Python 3.12** (pinned in `.python-version`; `pyproject.toml` requires `>=3.11`). If you have multiple Python versions installed, use the launcher to be explicit: `py -3.12` instead of bare `python`.

### macOS
- **A C++ compiler**: run `xcode-select --install` (Apple's Command Line Tools, includes Clang) if you don't already have one — CMake's configure step fails without it.
- **CMake 3.16+** — `pip install cmake`, or `brew install cmake`.
- **Python 3.12** recommended, `>=3.11` required. Homebrew Python, python.org installers, and pyenv can all coexist — know which `python`/`python3` your shell resolves to (`which python3`) before building, since the extension must be built against that same interpreter (see below).

### Linux
- **GCC 9+** or **Clang 10+** (`sudo apt install build-essential` on Debian/Ubuntu, or your distro's equivalent).
- **CMake 3.16+** — `pip install cmake` or your package manager.
- **Python 3.12** recommended, `>=3.11` required.

### The rule that avoids the most common failure: one Python, everywhere

**The compiled extension is ABI-specific to the exact Python that built it.**
Building with Python 3.12 and then running `uvicorn` under Python 3.10 (or a
different venv/virtualenv entirely) produces `ModuleNotFoundError: No module
named 'magnetics_cpp'` — the file exists, but that interpreter can't load it.
Pick one Python for this project (a single virtualenv is the simplest way)
and use it for every step below — the `cmake -S . -B ...` configure step,
`pip install`, and `python -m uvicorn` all need to agree.

If `cmake` (bare) isn't recognized as a command after `pip install cmake`,
its console scripts weren't put on your `PATH` — use `python -m cmake ...`
in every command below instead of bare `cmake ...` (works identically,
avoids the PATH problem entirely, and is safe to use even when `cmake` *is*
on PATH).

### Python packages
```bash
pip install pybind11
pip install -r python/requirements.txt   # fastapi, uvicorn[standard], requests
```
`pybind11` must be installed *before* running CMake — `CMakeLists.txt` calls `python -m pybind11 --cmakedir` to locate it.

The real core/material data (`data/real_materials.csv`, `data/real_cores.csv`)
is already checked into the repo — no extra install needed to run the app.
`PyOpenMagnetics` is only needed if you're regenerating that data via
`scripts/export_real_data.py`, and only runs on Linux/macOS/WSL2 — see
`docs/ARCHITECTURE.md` → "Data Source" for why.

---

## Build Instructions

Same commands on Windows, macOS, and Linux — CMake abstracts the platform
differences (MSVC vs. Clang vs. GCC). Use `python -m cmake` instead of
`cmake` throughout if bare `cmake` isn't on your `PATH` (see above).

### Step 1: Configure with CMake
```bash
# From the project root
cmake -S . -B build_pybind
```
Watch the output for `pybind11_DIR=...` and `Found pybind11: ...` — if
those are missing or it errors instead, `pybind11` isn't installed for the
Python this command is actually finding; re-check `pip install pybind11`
was run with the same `python`/`pip` you intend to use throughout.

### Step 2: Build the Python extension
```bash
cmake --build build_pybind --config Release --target magnetics_cpp
```
This compiles `magnetics_engine` → `magnetics_services` → the `magnetics_cpp` pybind11 module, and places the compiled extension directly in `python/` (per `CMakeLists.txt`'s `LIBRARY_OUTPUT_DIRECTORY` setting) — as `magnetics_cpp.cp312-win_amd64.pyd` on Windows, `magnetics_cpp.cpython-312-darwin.so` on macOS, or `magnetics_cpp.cpython-312-x86_64-linux-gnu.so` on Linux (version/platform tag varies with your actual Python) — where `python/routes/inductor_design.py` imports it as `import magnetics_cpp`. These build outputs are gitignored - each machine builds and keeps its own local copy; nothing here should be committed except the one pinned Vercel binary (see "Deploying to Vercel" below).

**Use `--config Release`, not `Debug`, especially on Windows.** `--config` only matters for multi-configuration generators (Visual Studio on Windows); on a single-config generator (Makefiles/Ninja, the Mac/Linux default) it's silently ignored, which is why this never showed up there. On Windows, MSVC's Debug configuration turns on full STL iterator/container checking (`_ITERATOR_DEBUG_LEVEL=2`), and this engine builds and copies a lot of `std::vector<...>` candidate lists and strings per request - under Debug that can run an order of magnitude slower than Release for no benefit (Phase 1 has no native debugger workflow that needs it). If the tool feels sluggish only on Windows, rebuild with `--config Release`.

**If you ever see `AttributeError: '...CoreData' object has no attribute 'X'`** (or any attribute) when starting the app, your compiled extension is stale — built from an older version of the C++ source that didn't have that field yet. Re-run Step 2 to rebuild it from the current source; this isn't a bug in the running code, it's a build artifact that's out of date with the checked-out branch.

### Step 3: Run the app
```bash
# Windows, if you have multiple Pythons installed:
py -3.12 -m uvicorn python.app:app --reload --host 127.0.0.1 --port 8000

# macOS/Linux, or Windows with only one Python / an activated venv:
python -m uvicorn python.app:app --reload --host 127.0.0.1 --port 8000
```
Use whichever `python`/`py -3.X` matches the interpreter Step 1/2 built
against — see the version-matching rule above.

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

## Optional: Local LLM Features (Natural-Language Input, Doc/Knowledge Assistant)

Two features — the "Describe It In Plain English" box in the web UI, and
the standalone `scripts/rag/` doc/knowledge assistant — need extra local
software beyond the app itself. Both are entirely optional; the core design
engine works without any of this.

- **[LM Studio](https://lmstudio.ai/)** running locally with its Local
  Server started (Developer tab → Local Server → Status: Running), a chat
  model loaded, and — for the RAG assistant only — an embedding model
  (`nomic-embed-text-v1.5` or similar) loaded too. Available on
  Windows, macOS, and Linux.
- **Docker** with a running `chromadb/chroma` container — needed only for
  `scripts/rag/` (`docker run -p 8000:8000 chromadb/chroma`), not for the
  natural-language input box.
- Install the extra Python dependency for the RAG scripts:
  `pip install -r scripts/rag/requirements.txt`.

See [RAG_ASSISTANT.md](RAG_ASSISTANT.md) for the full setup and usage of
the doc/knowledge assistant, and [FRONTEND_GUIDE.md](FRONTEND_GUIDE.md) for
the natural-language input box.

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