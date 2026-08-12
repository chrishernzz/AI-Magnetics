"""
magnetics_data.py:
cleans + converts + organizes data from the checked-in snapshot CSVs (data/real_materials.csv, data/real_cores.csv, data/real_core_loss_coefficients.csv, data/dc_bias_curves.csv) into clean Python Data objects
This is the only place in the codebase that knows about the CSV snapshot format; everything else (including the C++ engine) sees only the converted field names.

Loads real core and material data from a bundled snapshot file
(data/real_materials.csv, data/real_cores.csv) and maps it into the same
field names your C++ engine already expects.

DATA PROVENANCE: Every row in data/real_cores.csv and data/real_materials.csv was transcribed
by hand (created a python script that reads the datasheets), part-by-part, from Magnetics Inc.'s 
own live catalog (mag-inc.com'sAdvanced Part Number Finder: Powder Cores Search, Material=MPP + 
Shape=Toroid, and Material=All + Shape=E Core, Permeability=All each time)

Scope was deliberately narrowed to Magnetics powder High Flux toroids,
Kool Mu toroids, and MPP toroids and Magnetics powder E-cores only. Total is (1603 cores, 47 materials) 


Grading code: for MPP toroids, Magnetics' `00` (not graded, standard
tolerance) part number is used wherever it exists for a given (size,
permeability); `C0` (2%-band graded) is used only as a fallback where `00`
isn't offered at that permeability (several grades — e.g. 173μ, 300μ, 550μ —
are C0-only). E-cores only ever have a `00` grading code (Magnetics doesn't
offer graded E-cores).

E-core Wa (winding-window area) is now real data too: mag-inc.com's bulk part
search doesn't expose it (only external Length/LegLength/Width), but each
part's individual datasheet PDF does. Wa is a property of the physical body
only — verified by hand across every material/permeability variant sharing a
given (Le, Ae) pair (e.g. all of 1808's 26/40/60μ Kool Mu MAX variants report
the identical Wa=51.5mm2) — so one datasheet per unique physical size (18
sizes cover all 323 E-core rows) was enough, not one per part.

Mlt and WindowWidthMm/WindowHeightMm are now real for E-cores too: pulled
from each unique physical size's real Dimensions table (the "D"/"E"/"F"/"C"
labeled rows on the datasheet's isometric drawing). D/E were identified as
the window's height/width (not the outer body) by tolerance-type - A/B/C are
individually toleranced manufactured dimensions, D/E/L/M are Min/Nom
reference dimensions, consistent with a derived window opening - and
confirmed by internal consistency: E always exceeds the outer leg height B
(so E must be the window's WIDTH, not height, since a window can't be taller
than the leg it's cut into) across all 18 sizes. F*C (center-leg width x
depth) reproduces each size's real Ae within ~5% for 17 of 18 sizes (one
outlier, "3007" size code, off by ~18% - Mlt is already a documented
first-order estimate project-wide, not exact for any core), confirming F/C
as the center-leg cross-section used for Mlt = 2*(F+C).

Trade-off, stated plainly: this data is a snapshot, not live. If Magnetics'
catalog changes, someone has to re-pull it by hand the same way — this file
generation does not run automatically.
"""

import csv
from pathlib import Path

#starts from magnetics_data.py, go up through the project folders, find the data/ folder and then define the four database it needs which are below
DATA_DIR = Path(__file__).resolve().parents[2] / "data"
MATERIALS_FILE = DATA_DIR / "real_materials.csv"
CORES_FILE = DATA_DIR / "real_cores.csv"
CORE_LOSS_COEFFICIENTS_FILE = DATA_DIR / "real_core_loss_coefficients.csv"
DC_BIAS_CURVES_FILE = DATA_DIR / "dc_bias_curves.csv"

#check whether the CSV file exists and if it does not, raise a FileNotFoundEerror with a message. If the file exists, it opens the file and reads its contents using csv.DictReader, returning a list of dictionaries representing each row in the CSV file.
def _read_csv(path: Path) -> list[dict]:
    if not path.exists():
        raise FileNotFoundError(f"Real data snapshot not found: {path}. This file is checked into " f"the repo — if it's missing, something went wrong with the clone/copy, " f"not with your environment.")
    #DictReader reads the CSV file and returns each row as a dictionary, where the keys are the column headers and the values are the corresponding cell values for that row
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def fetch_materials(available_core_material_names: set[str] | None = None) -> list[dict]:
    """
    Returns real material records mapped to the same fields the C++ engine
    expects:
    Name, MuOpt, MinFrequencyHz, MaxFrequencyHz, Reason, Alternatives, BmaxT, Manufacturer, and DatasheetUrl

    BmaxT is real saturation flux density data as of this snapshot (see scripts/export_real_data.py) - CuLossFactor was retired the same way,
    replaced by data/real_core_loss_coefficients.csv (real Steinmetz coefficients, see fetch_core_loss_coefficients() below). The
    coefficients are loaded and searchable (findCoreLossCoefficients() in C++), but core loss itself is still not_evaluated in Phase 1 - the
    formula also needs a flux-density-swing value that isn't threaded through yet (see docs/FORMULAS.md).

    If available_core_material_names is given, materials not backed by at
    least one loaded core are skipped — otherwise MaterialSelection could
    recommend a material that CoreSelection has zero matching parts for.
    """
    #read from the MATERIALS_FILE CSV file and store the rows in a list of dictionaries
    rows = _read_csv(MATERIALS_FILE)
    results = []

    for r in rows:
        #check if available_core_material_names is provided and if the material name in the current row is not in that set. If both conditions are true, it skips to the next iteration of the loop, effectively filtering out materials that are not backed by at least one loaded core.
        if available_core_material_names is not None and r["Name"] not in available_core_material_names:
            continue
        results.append(
            {
                "Name": r["Name"],
                "MuOpt": float(r["MuOpt"]),
                "MinFrequencyHz": float(r["MinFrequencyHz"]),
                "MaxFrequencyHz": float(r["MaxFrequencyHz"]),
                "Reason": r["Reason"],
                "Alternatives": r["Alternatives"],
                "BmaxT": float(r["BmaxT"]),
                "Manufacturer": r.get("Manufacturer", ""),
                "DatasheetUrl": r.get("DatasheetUrl", ""),
            }
        )

    return results


def fetch_cores() -> list[dict]:
    """
    Returns real, assembled core parts mapped to the same fields the C++
    engine expects:
    PartNumber, Material, Mu, AL, Ae, Wa, Le, Mlt, PartCost, Vendor, MaxCurrent_A, MaxFreq_kHz, CoreShape, ShapeFamily, MaterialType, DatasheetUrl, WindowWidthMm, WindowHeightMm, SurfaceAreaWoundMm2, OdInches, IdInches, HtInches

    Mlt (mean-length-per-turn, mm) is real for MPP toroids (derived by hand from each part's real OD/ID/HT per mag-inc.com's own catalog data) and still a known gap for E-cores (see this file's module docstring).

    CoreShape ("Toroid"/"TwoPieceSet") and ShapeFamily ("T" for toroids, "E" for E-cores) are assigned by hand from each part's real mag-inc.com shape
    category - every row in this snapshot is one or the other, since scope is narrowed to just those two Magnetics powder shapes.

    MaterialType is always "powder" in this snapshot (MPP and every E-core material family - Kool Mu, Edge, XFlux, High Flux, etc. - are all
    distributed-gap powder materials) - the C++ engine uses this field (not CoreShape alone) to decide whether a core's permeability is a
    distributed-gap material property or needs the real machined-gap formula (ferrite only, none of which remains in this snapshot's scope) - see
    TurnsAndGapDesign.cpp.

    DatasheetUrl is real (~99% populated in this snapshot). WindowWidthMm/WindowHeightMm are the real rectangular winding-window dimensions,
    populated only for two-piece cores (100% coverage there) - toroids have no flat width/height (radial geometry instead), so these are genuinely
    blank ("0.0" here) for them, not missing data.
    """
    #read from the CORES_FILE CSV file and store the rows in a list of dictionaries
    rows = _read_csv(CORES_FILE)
    results = []

    for r in rows:
        results.append(
            {
                "PartNumber": r["PartNumber"],
                "Material": r["Material"],
                "Mu": float(r["Mu"]),
                "AL": float(r["AL"]),
                "Ae": float(r["Ae"]),
                "Wa": float(r["Wa"]),
                "Le": float(r["Le"]),
                "Mlt": float(r["Mlt"]),
                "PartCost": float(r["PartCost"]),
                "Vendor": r["Vendor"],
                "MaxCurrent_A": float(r["MaxCurrent_A"]),
                "MaxFreq_kHz": float(r["MaxFreq_kHz"]),
                "CoreShape": r.get("CoreShape", ""),
                "ShapeFamily": r.get("ShapeFamily", ""),
                "MaterialType": r.get("MaterialType", ""),
                "DatasheetUrl": r.get("DatasheetUrl", ""),
                "WindowWidthMm": float(r["WindowWidthMm"]) if r.get("WindowWidthMm") else 0.0,
                "WindowHeightMm": float(r["WindowHeightMm"]) if r.get("WindowHeightMm") else 0.0,
                #real, manufacturer-published wound-coil surface area (mm^2) from this part's own datasheet "Surface Area" table - see CoreDatabase.h's CoreData::surfaceAreaWoundMm2.
                #blank/0.0 means not yet transcribed, never a guessed/estimated value.
                "SurfaceAreaWoundMm2": float(r["SurfaceAreaWoundMm2"]) if r.get("SurfaceAreaWoundMm2") else 0.0,
                #OdInches, IdInches, HtInches are real, manufacturer-published external dimensions (inches) from this part's own datasheet "Dimensions" table - see CoreDatabase.h's CoreData::odInches/idInches/htInches.
                #blank/0.0 means not yet transcribed, never a guessed/estimated value
                "ODInches" :float(r["ODInches"]) if r.get("ODInches") else 0.0,
                "IDInches" :float(r["IDInches"]) if r.get("IDInches") else 0.0,
                "HTInches" :float(r["HTInches"]) if r.get("HTInches") else 0.0,
            }
        )

    return results


def fetch_core_loss_coefficients() -> list[dict]:
    """
    Returns real per-material Steinmetz coefficients, mapped to the same
    fields the C++ engine expects:
    MaterialName, MinFrequencyHz, MaxFrequencyHz, K, Alpha, Beta, Ct0, Ct1, Ct2

    Each material has one row per frequency range it was fit over (see
    data/real_core_loss_coefficients.csv). This replaces the retired
    CuLossFactor column - real_materials.csv no longer carries it.
    """
    #read from the CORE_LOSS_COEFFICIENTS_FILE CSV file and store the rows in a list of dictionaries
    rows = _read_csv(CORE_LOSS_COEFFICIENTS_FILE)
    results = []

    for r in rows:
        results.append(
            {
                "MaterialName": r["MaterialName"],
                "MinFreqHz": float(r["MinFrequencyHz"]),
                "MaxFreqHz": float(r["MaxFrequencyHz"]),
                "K": float(r["K"]),
                "Alpha": float(r["Alpha"]),
                "Beta": float(r["Beta"]),
                "Ct0": float(r["Ct0"]),
                "Ct1": float(r["Ct1"]),
                "Ct2": float(r["Ct2"]),
            }
        )

    return results


def fetch_dc_bias_curves() -> list[dict]:
    """
    Returns real powder-core DC-bias permeability roll-off curve-fit coefficients,
    mapped to the same fields the C++ engine expects:
    MaterialName, A, B, C, D, Vendor, DatasheetUrl

    %initial_permeability = 1/(A + B*H^C) + D, H in Oersteds - transcribed verbatim from Magnetics Inc.'s and Micrometals (adding soon)' own published "Permeability vs. DC Bias"
    datasheet pages (Toroid shape family) - see data/dc_bias_curves.csv. Only the materials this project has real transcribed coefficients for appear here; a
    distributed-gap material with no row here falls back to the flat catalog AL in TurnsAndGapDesign.cpp, never a guessed curve.
    """
    #read from the DC_BIAS_CURVES_FILE CSV file and store the rows in a list of dictionaries
    rows = _read_csv(DC_BIAS_CURVES_FILE)
    results = []

    for r in rows:
        results.append(
            {
                "MaterialName": r["MaterialName"],
                "A": float(r["A"]),
                "B": float(r["B"]),
                "C": float(r["C"]),
                "D": float(r["D"]),
                "Vendor": r.get("Vendor", ""),
                "DatasheetUrl": r.get("DatasheetUrl", ""),
            }
        )

    return results

#this function will fetch all the cores and materials, and return them as a tuple of two lists of dictionaries
def fetch_all() -> tuple[list[dict], list[dict]]:
    """
    The function app.py actually calls at startup. Returns (materials, cores), with materials restricted to
    ones the loaded cores actually use. Same signature as before — app.py did not need to change.
    """
    #fetch all cores first, then extract the unique material names used by those cores. Then fetch the materials, filtering to only include those that are actually used by the loaded cores. Finally, return both the materials and cores as a tuple of two lists of dictionaries.
    cores = fetch_cores()
    #duplicates are removed by using a set comprehension to collect the "Material" values from each core dictionary in the core lists
    used_material_names = {
        c["Material"]
        for c in cores
    }
    #now loads only materials that are actually used by the loaded cores, filtering out any unused materials from the final result
    materials = fetch_materials(available_core_material_names=used_material_names)

    return materials, cores


if __name__ == "__main__":
    materials, cores = fetch_all()

    #debugging: only printing the first 5 materials and cores to avoid overwhelming output but helps to check that it reads the data correctly
    print(f"Materials: {len(materials)}")
    for m in materials[:5]:
        print(" ", m)

    print(f"\nCores: {len(cores)}")
    for c in cores[:5]:
        print(" ", c)