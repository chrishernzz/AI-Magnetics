"""
magnetics_data.py

Loads real core and material data from a bundled snapshot file
(data/real_materials.csv, data/real_cores.csv) and maps it into the same
field names your C++ engine already expects.

DATA PROVENANCE: this project no longer uses PyOpenMagnetics/MAS in any
form, at build time or runtime - not as a dependency, not as a data source.
Every row in data/real_cores.csv and data/real_materials.csv was transcribed
by hand, part-by-part, from Magnetics Inc.'s own live catalog (mag-inc.com's
Advanced Part Number Finder: Powder Cores Search, Material=MPP + Shape=Toroid,
and Material=All + Shape=E Core, Permeability=All each time), after a database
review found real errors in the old PyOpenMagnetics-sampled snapshot this
project used to ship (missing permeability grades, and cores whose catalog
number didn't actually correspond to the geometry/permeability recorded).
There is no PyOpenMagnetics import anywhere in this codebase anymore, and no
script that regenerates this data from it - if you need to refresh this
snapshot, you re-pull it from mag-inc.com by hand the same way, there's no
automated substitute.

Scope was deliberately narrowed to Magnetics powder MPP toroids and Magnetics
powder E-cores only (755 cores, 34 materials) — every other family the old
snapshot carried (Kool Mu/XFlux/High Flux/Edge toroids, all ferrite including
the old E100/60/28-3C90 fixture, U/EQ/LP/EER/Blocks/THINZ shapes) was removed
as out of scope, not because it was wrong.

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

DATA_DIR = Path(__file__).resolve().parents[2] / "data"
MATERIALS_FILE = DATA_DIR / "real_materials.csv"
CORES_FILE = DATA_DIR / "real_cores.csv"
CORE_LOSS_COEFFICIENTS_FILE = DATA_DIR / "real_core_loss_coefficients.csv"
DC_BIAS_CURVES_FILE = DATA_DIR / "dc_bias_curves.csv"


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
                "Manufacturer": r.get("Manufacturer", ""),
                "DatasheetUrl": r.get("DatasheetUrl", ""),
            }
        )

    return results


def fetch_cores() -> list[dict]:
    """
    Returns real, assembled core parts mapped to the same fields the C++
    engine expects:

    PartNumber, Material, Mu, AL, Ae, Wa, Le, Mlt,
    PartCost, Vendor, MaxCurrent_A, MaxFreq_kHz, CoreShape, ShapeFamily

    Mlt (mean-length-per-turn, mm) is real for MPP toroids (derived by hand
    from each part's real OD/ID/HT per mag-inc.com's own catalog data) and
    still a known gap for E-cores (see this file's module docstring).

    CoreShape ("Toroid"/"TwoPieceSet") and ShapeFamily ("T" for toroids, "E"
    for E-cores) are assigned by hand from each part's real mag-inc.com shape
    category - every row in this snapshot is one or the other, since scope is
    narrowed to just those two Magnetics powder shapes.

    MaterialType is always "powder" in this snapshot (MPP and every E-core
    material family - Kool Mu, Edge, XFlux, High Flux, etc. - are all
    distributed-gap powder materials) - the C++ engine uses this field (not
    CoreShape alone) to decide whether a core's permeability is a
    distributed-gap material property or needs the real machined-gap formula
    (ferrite only, none of which remains in this snapshot's scope) - see
    TurnsAndGapDesign.cpp.

    DatasheetUrl is real (~99% populated in this snapshot). WindowWidthMm/
    WindowHeightMm are the real rectangular winding-window dimensions,
    populated only for two-piece cores (100% coverage there) - toroids have
    no flat width/height (radial geometry instead), so these are genuinely
    blank ("0.0" here) for them, not missing data.
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
                "MaterialType": r.get("MaterialType", ""),
                "DatasheetUrl": r.get("DatasheetUrl", ""),
                "WindowWidthMm": float(r["WindowWidthMm"]) if r.get("WindowWidthMm") else 0.0,
                "WindowHeightMm": float(r["WindowHeightMm"]) if r.get("WindowHeightMm") else 0.0,
                # Real, manufacturer-published AL tolerance (+-%) from this part's own datasheet - see
                # CoreDatabase.h's CoreData::alTolerancePercent. Blank/0.0 means not yet transcribed for
                # this part - as of this column's introduction only C055439A2 (8%) is populated; every
                # other row is a genuine, honest gap, never a guessed blanket percentage.
                "AlTolerancePercent": float(r["AlTolerancePercent"]) if r.get("AlTolerancePercent") else 0.0,
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


def fetch_dc_bias_curves() -> list[dict]:
    """
    Returns real powder-core DC-bias permeability roll-off curve-fit coefficients,
    mapped to the same fields the C++ engine expects:

    MaterialName, A, B, C, D, Vendor, DatasheetUrl

    %initial_permeability = 1/(A + B*H^C) + D, H in Oersteds - transcribed verbatim
    from Magnetics Inc.'s and Micrometals' own published "Permeability vs. DC Bias"
    datasheet pages (Toroid shape family) - see data/dc_bias_curves.csv. Only the
    materials this project has real transcribed coefficients for appear here; a
    distributed-gap material with no row here falls back to the flat catalog AL in
    TurnsAndGapDesign.cpp, never a guessed curve.
    """
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