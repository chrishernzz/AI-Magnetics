"""
Loads the same real material/core snapshot python/app.py's FastAPI startup
hook loads, so tests/python/*.py can call magnetics_cpp.run_inductor_design
directly without spinning up the HTTP server. Mirrors
python/app.py:load_real_magnetics_data() - kept separate since app.py's
version is a FastAPI startup hook, not an importable helper.
"""
import sys
from pathlib import Path

import pytest

PYTHON_DIR = Path(__file__).resolve().parents[2] / "python"
sys.path.insert(0, str(PYTHON_DIR))


@pytest.fixture(scope="session", autouse=True)
def load_real_data():
    import magnetics_cpp
    from services.magnetics_data import fetch_all

    materials, cores = fetch_all()

    cpp_materials = []
    for m in materials:
        record = magnetics_cpp.MaterialData()
        record.name = m["Name"]
        record.muOpt = m["MuOpt"]
        record.minFrequencyHz = m["MinFrequencyHz"]
        record.maxFrequencyHz = m["MaxFrequencyHz"]
        record.reason = m["Reason"]
        record.alternatives = m["Alternatives"]
        record.bmaxT = m["BmaxT"]
        cpp_materials.append(record)

    cpp_cores = []
    for c in cores:
        record = magnetics_cpp.CoreData()
        record.partNumber = c["PartNumber"]
        record.material = c["Material"]
        record.mu = c["Mu"]
        record.al = c["AL"]
        record.ae = c["Ae"]
        record.wa = c["Wa"]
        record.le = c["Le"]
        record.mlt = c["Mlt"]
        cpp_cores.append(record)

    magnetics_cpp.set_material_database(cpp_materials)
    magnetics_cpp.set_core_database(cpp_cores)
