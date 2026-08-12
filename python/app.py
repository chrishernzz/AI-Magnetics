#type: ignore
"""
app.py:
starts the FastAPI backend, serves the frontend, loads the magnetics database when the application starts, converts python magnetics_data.py 
data into C++ objects, give those objects to the C++ engine, and connects the API routes like (inductor_design.py and topology_desing.py)
"""
#used here to modify where Python looks for modules
import sys
#used for working with file/folder paths
from pathlib import Path
#creates the backend web application
from fastapi import FastAPI
#sends actual files back to the browser like index.html
from fastapi.responses import FileResponse
#allows FastAPI to serve files such as Js, Css, and Images
from fastapi.staticfiles import StaticFiles

#look in the directory containing this python file when we are trying to import modules where __file__ contains the current python file (getting the path of this file)
sys.path.insert(0, str(Path(__file__).resolve().parent))
#this now creates the path to the frontend directory
frontend_dir = Path(__file__).resolve().parent.parent / "frontend"
#this creates the actaul API server
app = FastAPI(title="AIMagnetics Python API",version="0.1.0",)

#creating my own modified version of FastAPI StaticFiles
class NoCacheStaticFiles(StaticFiles):
    """Plain StaticFiles sends no Cache-Control header at all, which leaves browsers free to apply their own heuristic caching (commonly based on
    Last-Modified) and can silently keep serving an old app.js/styles.css after a git pull with no way to tell short of comparing bytes by hand.
    Forcing revalidation on every request costs one cheap conditional GET 304 if unchanged) and makes that entire failure mode impossible."""

    #before using my cached version, this checks whether the server has a newer version and then sends the response
    def file_response(self, *args, **kwargs):
        response = super().file_response(*args, **kwargs)
        response.headers["Cache-Control"] = "no-cache"
        return response

#this is telling the FastAPI to serve the files from the frontend folder under /static
app.mount("/static", NoCacheStaticFiles(directory=frontend_dir), name="static")

#converts python dictionaries into C++ Objects: first parameter = a list of dictionary, cpp_class = tells the function what C++ objects to create for materials it will be magnetics_cpp.MaterialData, third parameter = field_map that tells python dictionary key belongs to which C++ field
def _build_cpp_record(dicts: list[dict], cpp_class, field_map: dict[str, str]) -> list:
    """Converts a list of plain dicts (from magnetics_data.py) into a list of pybind11-exposed C++ objects. field_map says which dict key goes to 
    which C++ attribute, e.g. {"muOpt": "MuOpt"}. This replaces what used to be two seperate, nearly-identical loops (one for materials, one for cores)
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
    Loads real core/material data from the checked-in CSV snapshot once, at process startup, and hands it to the C++ engine. There is no fallback - if this fails, it
    raises and uvicorn will refuse to start, rather than silently starting an app with no core/material data. Check the traceback below to see exactly what failed
    (a missing/corrupt data/*.csv file is the most common cause).
    """
    import hashlib
    #this will allow communication with my C++ code 
    import magnetics_cpp
    #use the magnetics_data.py file in servies and call all the functions in here
    from services.magnetics_data import fetch_all, fetch_core_loss_coefficients, fetch_dc_bias_curves, CORES_FILE, MATERIALS_FILE

    #load all the different datasets 
    materials, cores = fetch_all()
    core_loss_coefficients = fetch_core_loss_coefficients()
    dc_bias_curves = fetch_dc_bias_curves()
    #base case that checks if materials or cores are empty data then error 
    if not materials or not cores:
        raise RuntimeError(f"Real data load returned {len(materials)} materials, {len(cores)} cores - " f"refusing to start with an empty database. Check the filters in" f"python/services/magnetics_data.py (ALLOWED_MATERIAL_TYPES, size range, etc.).")

    #Unlike materials/cores above, an empty core-loss-coefficient database is not fatal - though as of this snapshot it should never actually be empty: real coefficients now exist for all 34 materials (MPP's 12
    #permeability grades plus all 22 E-core material/permeability combinations - Kool Mu, Kool Mu HF/MAX/ Ultra, Edge, XFlux, XFlux Ultra, High Flux), each transcribed by hand from that material's own
    #mag-inc.com Core Loss Density Curves fit-formula table (the E Cores/U Cores/EER Cores shape rows specifically for the E-core families, which publish different coefficients per shape - not the Toroids
    #rows) and unit-converted from Magnetics' P[mW/cm3]=a*B^b*f[kHz]^c form to this engine's Pv[W/m3]=k*f[Hz]^alpha*B^beta form. 
    #This check stays defensive rather than a hard requirement, since a future edit could still leave it empty by mistake - that should still warn, not crash.
    if not core_loss_coefficients:
        print("WARNING: real data load returned 0 core-loss coefficient rows - core loss will report " "not_evaluated for every material. Expected for the current MPP/powder-only database scope " "(see data/real_core_loss_coefficients.csv header).")
    #this will convert the materials into C++ objects
    cpp_materials = _build_cpp_record(materials, magnetics_cpp.MaterialData, {
        "name" : "Name",
        "muOpt" : "MuOpt",
        "minFrequencyHz" : "MinFrequencyHz",
        "maxFrequencyHz" : "MaxFrequencyHz",
        "reason" : "Reason",
        "alternatives" : "Alternatives",
        "bmaxT" : "BmaxT",
        "manufacturer" : "Manufacturer",
        "datasheetUrl" : "DatasheetUrl",
    })
    #this will convert the cores into C++ objects
    cpp_cores = _build_cpp_record(cores, magnetics_cpp.CoreData, {
        "partNumber" : "PartNumber",
        "material" : "Material",
        "mu" : "Mu",
        "al" : "AL",
        "ae" : "Ae",
        "wa" : "Wa",
        "le" : "Le",
        "mlt" : "Mlt",
        "vendor" : "Vendor",
        "coreShape" : "CoreShape",
        "shapeFamily" : "ShapeFamily",
        "materialType" : "MaterialType",
        "datasheetUrl" : "DatasheetUrl",
        "windowWidthMm" : "WindowWidthMm",
        "windowHeightMm" : "WindowHeightMm",
        "surfaceAreaWoundMm2" : "SurfaceAreaWoundMm2",
        "odInches" : "ODInches",
        "idInches" : "IDInches",
        "htInches" : "HTInches",
    })
    #this will convert the core loss coef into C++ objects
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
    #Unlike materials/cores/core-loss-coefficients above, an empty DC-bias curve database is not fatal - #TurnsAndGapDesign.cpp's distributed-gap branch already falls back to the flat catalog AL (the original
    #Phase 1 behavior) for any powder material with no published curve, so this data is a real accuracy #improvement for the materials it covers, not a hard dependency for the app to start.
    cpp_dc_bias_curves = _build_cpp_record(dc_bias_curves, magnetics_cpp.DCBiasCurveData, {
        "materialName" : "MaterialName",
        "a" : "A",
        "b" : "B",
        "c" : "C",
        "d" : "D",
        "vendor" : "Vendor",
        "datasheetUrl" : "DatasheetUrl",
    })
    #this is now saying C++ engine here is your materials, cores, core loss coef, and dc bias curve database and now has everything loaded into memory
    magnetics_cpp.set_material_database(cpp_materials)
    magnetics_cpp.set_core_database(cpp_cores)
    magnetics_cpp.set_core_loss_coefficient_database(cpp_core_loss_coefficients)
    magnetics_cpp.set_dc_bias_curve_database(cpp_dc_bias_curves)

    #Real, reproducible version identifiers - a content hash of the exact CSV bytes actually loaded, not an invented semver for data whose upstream (Magnetics Inc.'s own catalog) release version isn't
    #tracked anywhere in this snapshot mechanism. Changes if and only if the underlying data actually changes.
    core_db_version = f"{len(cpp_cores)} rows, sha256:{hashlib.sha256(CORES_FILE.read_bytes()).hexdigest()[:12]}"
    material_db_version = f"{len(cpp_materials)} rows, sha256:{hashlib.sha256(MATERIALS_FILE.read_bytes()).hexdigest()[:12]}"
    magnetics_cpp.set_engine_versions(core_db_version, material_db_version)

    #this is the output you see when the application starts
    print(f"Loaded real data: {len(cpp_materials)} materials, {len(cpp_cores)} cores, "
          f"{len(cpp_core_loss_coefficients)} core-loss coefficient rows, "
          f"{len(cpp_dc_bias_curves)} DC-bias roll-off curve rows. source: Magnetics Inc. datasheets "
          f"(mag-inc.com) - MPP toroids + E-cores only")


@app.get("/", response_class=FileResponse)
def read_index():
    return FileResponse(frontend_dir / "index.html", headers={"Cache-Control": "no-cache"})

#getting the endpoints from the folder routes inside python
from routes.inductor_design import router as inductor_design_router
from routes.topology_design import router as topology_design_router
app.include_router(inductor_design_router)
app.include_router(topology_design_router)