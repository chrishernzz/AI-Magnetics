If you did not change any C++ code or bindings:
Just start the Python app: 
cd "C:\Users\chernandez1\OneDrive - AMETEK Inc\Desktop\AIMagnetics"
python -m uvicorn python.app:app --reload --host 127.0.0.1 --port 8000
That is the normal run flow now.

If you changed C++ code or the binding file:
Rebuild the Python extension first, then run Uvicorn:
cd "C:\Users\chernandez1\OneDrive - AMETEK Inc\Desktop\AIMagnetics"
python -m cmake -S . -B build_pybind
python -m cmake --build build_pybind --config Debug --target magnetics_cpp
python -m uvicorn python.app:app --reload --host 127.0.0.1 --port 8000