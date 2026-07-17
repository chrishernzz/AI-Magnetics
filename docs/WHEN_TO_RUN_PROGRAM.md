# When to Rebuild vs. Just Run

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
python -m cmake --build build_pybind --config Debug --target magnetics_cpp
python -m uvicorn python.app:app --reload --host 127.0.0.1 --port 8000
```

## If you only changed a CSV file in data/

No rebuild needed — the C++ engine reads CSVs at runtime. Just restart uvicorn (Ctrl+C, then re-run the same command) so the new file gets 
picked up.