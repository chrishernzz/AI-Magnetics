#type: ignore
import sys
from pathlib import Path
from fastapi import FastAPI
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles


sys.path.insert(0, str(Path(__file__).resolve().parent))
frontend_dir = Path(__file__).resolve().parent.parent / "frontend"

app = FastAPI(title="AIMagnetics Python API",version="0.1.0",)


class NoCacheStaticFiles(StaticFiles):
    """Plain StaticFiles sends no Cache-Control header at all, which leaves
    browsers free to apply their own heuristic caching (commonly based on
    Last-Modified) and can silently keep serving an old app.js/styles.css
    after a git pull with no way to tell short of comparing bytes by hand.
    Forcing revalidation on every request costs one cheap conditional GET
    (304 if unchanged) and makes that entire failure mode impossible."""

    def file_response(self, *args, **kwargs):
        response = super().file_response(*args, **kwargs)
        response.headers["Cache-Control"] = "no-cache"
        return response


app.mount("/static", NoCacheStaticFiles(directory=frontend_dir), name="static")

def _build_cpp_record(dicts: list[dict], cpp_class, field_map: dict[str, str]) -> list:
    """Converts a list of plain dicts (from magnetics_data.py) into a list of
    pybind11-exposed C++ objects. field_map says which dict key goes to 
    which C++ attribute, e.g. {"muOpt": "MuOpt"}. This replaces what used
    to be two seperate, nearly-identical loops (one for materials, one for cores)
    with one function used twice.
    """
    records = []
    for d in dicts:
        record = cpp_class()
        for cpp_attr, dict_key in field_map.items():
            setattr(record, cpp_attr, d[dict_key])
        records.append(record)
    return records

@app.on_event("startup")
def load_real_magnetics_data():
    """
    Loads real core/material data from PyOpenMagnetics once, at process startup,
    and hands it to the C++ engine. There is no CSV fallback - if this fails, it raises and uvicorn will refuse to start,
    rather than silently starting an app with no core/material data. Check the
    traceback below to see exactly what failed (missing PyOpenMagnetics install is the most common cause).
    """
    import magnetics_cpp
    from services.magnetics_data import fetch_all, fetch_core_loss_coefficients

    materials, cores = fetch_all()
    core_loss_coefficients = fetch_core_loss_coefficients()

    if not materials or not cores:
        raise RuntimeError(
            f"Real data load returned {len(materials)} materials, {len(cores)} cores - "
            f"refusing to start with an empty database. Check the filters in"
            f"python/services/magnetics_data.py (ALLOWED_MATERIAL_TYPES, size range, etc.)."
        )
    if not core_loss_coefficients:
        raise RuntimeError(
            "Real data load returned 0 core-loss coefficient rows - refusing to start with an "
            "empty database. Check data/real_core_loss_coefficients.csv."
        )
    cpp_materials = _build_cpp_record(materials, magnetics_cpp.MaterialData, {
        "name" : "Name",
        "muOpt" : "MuOpt",
        "minFrequencyHz" : "MinFrequencyHz",
        "maxFrequencyHz" : "MaxFrequencyHz",
        "reason" : "Reason",
        "alternatives" : "Alternatives",
        "bmaxT" : "BmaxT",
    })
    cpp_cores = _build_cpp_record(cores, magnetics_cpp.CoreData, {
        "partNumber" : "PartNumber",
        "material" : "Material",
        "mu" : "Mu",
        "al" : "AL",
        "ae" : "Ae",
        "wa" : "Wa",
        "le" : "Le",
        "mlt" : "Mlt",
    })
    cpp_core_loss_coefficients = _build_cpp_record(core_loss_coefficients, magnetics_cpp.CoreLossCoefficientData, {
        "materialName" : "MaterialName",
        "minFreqHz" : "MinFreqHz",
        "maxFreqHz" : "MaxFreqHz",
        "k" : "K",
        "alpha" : "Alpha",
        "beta" : "Beta",
        "ct0" : "Ct0",
        "ct1" : "Ct1",
        "ct2" : "Ct2",
    })
    magnetics_cpp.set_material_database(cpp_materials)
    magnetics_cpp.set_core_database(cpp_cores)
    magnetics_cpp.set_core_loss_coefficient_database(cpp_core_loss_coefficients)
    print(f"Loaded real data: {len(cpp_materials)} materials, {len(cpp_cores)} cores, "
          f"{len(cpp_core_loss_coefficients)} core-loss coefficient rows. source: PyOpenMagnetics / MAS")


@app.get("/", response_class=FileResponse)
def read_index():
    return FileResponse(frontend_dir / "index.html", headers={"Cache-Control": "no-cache"})


from routes.inductor_design import router as inductor_design_router
from routes.topology_design import router as topology_design_router
app.include_router(inductor_design_router)
app.include_router(topology_design_router)