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

## If you changed C++ code or the binding file

Rebuild the Python extension first, then run uvicorn:
```bash
cd /path/to/AIMagnetics
python -m cmake -S . -B build_pybind
python -m cmake --build build_pybind --config Release --target magnetics_cpp
python -m uvicorn python.app:app --reload --host 127.0.0.1 --port 8000
```

If you skip the rebuild after a C++ change and start the app anyway, the
symptom is usually a cryptic `AttributeError` naming a field that "doesn't
exist" on startup - the running binary is just older than the source you're
looking at. See GETTING_STARTED.md's Step 2 note for the exact message.

## If you only changed a CSV file in data/

No rebuild needed — the C++ engine reads CSVs at runtime. Just restart uvicorn (Ctrl+C, then re-run the same command) so the new file gets 
picked up.

## If you only changed a file in docs/

No rebuild, and no uvicorn restart either — `docs/` isn't read by any code
at all.