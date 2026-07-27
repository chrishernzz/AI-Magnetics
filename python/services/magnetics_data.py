"""
magnetics_data.py

Loads real core and material data from a bundled snapshot file
(data/real_materials.csv, data/real_cores.csv) and maps it into the same
field names your C++ engine already expects.

WHY A BUNDLED FILE INSTEAD OF A LIVE PYOPENMAGNETICS QUERY:
PyOpenMagnetics does not support native Windows (only Linux/macOS, or
Windows via WSL2 — see the project's own docs/compatibility.md), and has
no published wheel for Python 3.14 on any platform. Since this project
runs on native Windows Python 3.14, importing PyOpenMagnetics at runtime
is not currently possible here.

The data is still 100% real — sourced from PyOpenMagnetics/MAS (real
manufacturer datasheets: Ferroxcube, TDK, Magnetics, Fair-Rite) — it was
generated once, in an environment where PyOpenMagnetics does install
(Linux), and the output was checked into this repo as a snapshot. This
file is the only thing that changed to make that work; everything
downstream (DataCache, CoreDatabase, Materials, app.py's startup hook)
is unchanged and doesn't know or care where the data came from.

Trade-off, stated plainly: this data is a snapshot, not live. To refresh
it, re-run the export (see scripts/export_real_data.py) somewhere
PyOpenMagnetics actually installs, and replace the two CSV files below.

Snapshot generated: see data/real_materials.csv / data/real_cores.csv
header comment for the export date. 32 materials, 60 cores as of this
snapshot — filtered to power-application ferrite/powder materials,
ungapped cores only (GapDesign.cpp isn't implemented yet), spread across
materials and vendors (not just the first N encountered).
"""

import csv
from pathlib import Path

DATA_DIR = Path(__file__).resolve().parents[2] / "data"
MATERIALS_FILE = DATA_DIR / "real_materials.csv"
CORES_FILE = DATA_DIR / "real_cores.csv"
CORE_LOSS_COEFFICIENTS_FILE = DATA_DIR / "real_core_loss_coefficients.csv"


def _read_csv(path: Path) -> list[dict]:
    if not path.exists():
        raise FileNotFoundError(
            f"Real data snapshot not found: {path}. This file is checked into "
            f"the repo — if it's missing, something went wrong with the clone/copy, "
            f"not with your environment."
        )

    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def fetch_materials(available_core_material_names: set[str] | None = None) -> list[dict]:
    """
    Returns real material records mapped to the same fields the C++ engine
    expects:

    Name, MuOpt, MinFrequencyHz, MaxFrequencyHz, Reason,
    Alternatives, BmaxT

    BmaxT is real saturation flux density data as of this snapshot (see
    scripts/export_real_data.py) - CuLossFactor was retired the same way,
    replaced by data/real_core_loss_coefficients.csv (real Steinmetz
    coefficients, see fetch_core_loss_coefficients() below). The
    coefficients are loaded and searchable (findCoreLossCoefficients() in
    C++), but core loss itself is still not_evaluated in Phase 1 - the
    formula also needs a flux-density-swing value that isn't threaded
    through yet (see docs/FORMULAS.md).

    If available_core_material_names is given, materials not backed by at
    least one loaded core are skipped — otherwise MaterialSelection could
    recommend a material that CoreSelection has zero matching parts for.
    """
    rows = _read_csv(MATERIALS_FILE)

    results = []

    for r in rows:
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
            }
        )

    return results


def fetch_cores() -> list[dict]:
    """
    Returns real, assembled core parts mapped to the same fields the C++
    engine expects:

    PartNumber, Material, Mu, AL, Ae, Wa, Le, Mlt,
    PartCost, Vendor, MaxCurrent_A, MaxFreq_kHz, CoreShape, ShapeFamily

    Mlt (mean-length-per-turn, mm) is a real-geometry estimate as of this
    snapshot - see scripts/export_real_data.py's module docstring for
    exactly what it does and doesn't account for.

    CoreShape ("Toroid"/"TwoPieceSet") and ShapeFamily (e.g. "T", "ETD",
    "PQ") are real geometry classifications from PyOpenMagnetics - see
    scripts/export_real_data.py's _core_shape_and_family().
    """
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


def fetch_all() -> tuple[list[dict], list[dict]]:
    """
    The function app.py actually calls at startup.
    Returns (materials, cores), with materials restricted to
    ones the loaded cores actually use.
    Same signature as before — app.py did not need to change.
    """
    cores = fetch_cores()
    used_material_names = {
        c["Material"]
        for c in cores
    }
    materials = fetch_materials(available_core_material_names=used_material_names)

    return materials, cores


if __name__ == "__main__":
    mats, cores = fetch_all()

    print(f"Materials: {len(mats)}")

    for m in mats[:5]:
        print(" ", m)

    print(f"\nCores: {len(cores)}")

    for c in cores[:5]:
        print(" ", c)