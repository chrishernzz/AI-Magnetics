# When to Rebuild vs. Just Run

`python` below means whichever interpreter you built `magnetics_cpp`
against — see GETTING_STARTED.md's "one Python, everywhere" rule. On
Windows with multiple Pythons installed, that's usually `py -3.12` instead
of bare `python`; on macOS/Linux with an activated virtualenv, `python`
inside that venv is normally correct.

## If you did NOT change any C++ code or the pybind11 binding file

Just start the Python app:
```bash
cd /path/to/AIMagnetics
python -m uvicorn python.app:app --reload --host 127.0.0.1 --port 8000
```
That's the normal run flow.

## If you changed C++ code or the binding file (or pulled/merged a branch that did)

This applies just as much after `git pull`/`git fetch ... branch:branch` +
merge as it does after editing the code yourself - **pulling in someone
else's commits never rebuilds the compiled extension for you.** If the
branch you just merged touched anything under `src/` or
`src/python_bindings/InductorDesignBindings.cpp`, treat it exactly like a
local C++ change and rebuild before running.

Before rebuilding for the first time (or on a fresh clone), make sure
`cmake` and `pybind11` are actually installed for *this* interpreter - see
GETTING_STARTED.md's "Python packages" section. Installing them under a
different `python`/`py -3.X` than the one you rebuild with fails with
`No module named cmake` (or `pybind11`), and on Windows that error can
scroll by without stopping the rest of a pasted multi-line command block,
so it's easy to not notice the rebuild never actually happened.

Rebuild the Python extension first, then run uvicorn:
```bash
cd /path/to/AIMagnetics
python -m cmake -S . -B build_pybind
python -m cmake --build build_pybind --config Release --target magnetics_cpp
python -m uvicorn python.app:app --reload --host 127.0.0.1 --port 8000
```
Confirm the second command's output ends with `Built target magnetics_cpp`
before moving on - if it printed an error instead, fix that first (most
often the missing-package issue above), then re-run it. Restarting uvicorn
after a build that silently failed just reloads the same stale binary and
reproduces the same error.

If you skip the rebuild (or the rebuild silently failed) after a C++
change and start the app anyway, the symptom is a cryptic `AttributeError`
mentioning `magnetics_cpp` on startup or on the first request that uses
the new code - either a field "doesn't exist" on an existing binding, or
the module itself has no attribute for a function/class a newer commit
added. Either way the running binary is just older than the source you're
looking at. See GETTING_STARTED.md's Step 2 note for both exact message
shapes.

## If you only changed a CSV file in data/

No rebuild needed — the C++ engine reads CSVs at runtime. Just restart uvicorn (Ctrl+C, then re-run the same command) so the new file gets 
picked up.

## If you only changed a file in docs/

No rebuild, and no uvicorn restart either — `docs/` isn't read by any code
at all.