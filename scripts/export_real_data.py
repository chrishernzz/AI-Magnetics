"""
export_real_data.py

Maintenance script — NOT part of the running app. Run this to regenerate
data/real_materials.csv, data/real_cores.csv, and
data/real_core_loss_coefficients.csv from a fresh PyOpenMagnetics query.

Must be run somewhere PyOpenMagnetics actually installs — Linux, macOS, or
Windows via WSL2. It will not run on native Windows (see docs/ARCHITECTURE.md
-> "Data Source: Real Data, No Live Windows Dependency" for why).

Usage:
    pip install PyOpenMagnetics
    python scripts/export_real_data.py

This overwrites all three data/ files above. The running app
(python/services/magnetics_data.py) reads those files — it never imports
PyOpenMagnetics itself.

BmaxT (saturation) and the Steinmetz core-loss coefficients below are real
values read from PyOpenMagnetics' own material records - previously this
script hardcoded both to 0.0 as unpopulated placeholders even though the
upstream library already had them. Mean-length-per-turn (Mlt) is not a
direct field anywhere upstream; it's derived here from each core's real
central-column cross-section (treated as the wire's innermost turn, ignoring
bobbin wall thickness and winding build-up) - a first-order estimate from
real geometry, not a guess, but a documented simplification the same way the
gap formula's fringing-flux omission is documented in FORMULAS.md.

CoreShape/ShapeFamily (added after a real user report - see git log) are
read from PyOpenMagnetics' functionalDescription.shape: magneticCircuit
"closed" -> "Toroid", "open" -> "TwoPieceSet" (E-core, ETD, PQ, RM, etc.),
and shape.family (e.g. "t", "etd", "pq") uppercased for a human-readable
geometry label.

MaterialType ("ferrite"/"powder", added after a real user report - a
ferrite toroid, e.g. N87, was being treated as a distributed-gap powder
core purely because CoreShape=="Toroid") is read directly from
PyOpenMagnetics' own material.material field - the exact same field
ALLOWED_MATERIAL_TYPES already filters on below, just never previously
carried through to the CSV output. TurnsAndGapDesign.cpp uses this
(materialType=="powder", not shape alone) to decide whether a core's
effective permeability is a distributed-gap material property (no
discrete machined gap exists) or needs the real machined-gap formula -
see DATA_FILES.md.

DatasheetUrl/Manufacturer (materials) and DatasheetUrl (cores) - added
after auditing what PyOpenMagnetics actually carries vs. what this script
previously captured: real datasheet URLs exist for 95.1% of cores and
98.0% of materials upstream, and every material has a real manufacturer
name, none of which were being read before (SourceInfo.datasheetUrl was
always left std::nullopt, documented as "no such data exists" - it did,
this script just never asked for it). Read straight from
manufacturerInfo.datasheetUrl/name - "" (not a guess) when genuinely
absent for a specific part.

WindowWidthMm/WindowHeightMm (cores) - real rectangular winding-window
dimensions, not just the window's total area (Wa). PyOpenMagnetics has
these for 100% of two-piece (open-shape) cores; toroids have no flat
width/height (their window is described by radialHeight instead), so
this is genuinely "" for them, not a missing-data bug. Closes part of the
gap WindingDesign.cpp's honesty-ledger comments describe ("core data has
no width/height split, only a raw window area") for two-piece cores -
not yet consumed by the physical-fill formula itself, which still uses
the area-fraction margin/lead-exit estimates; that's a separate decision
for whether/how to use a real linear dimension there.

The per-vendor cap used to be a single flat counter, which meant whichever
material family happened to iterate first for a vendor could consume the
entire quota before other real, qualifying materials from that same vendor
were ever considered - discovered when a real MPP toroid (Magnetics
C055439A2X2) was found completely absent from the snapshot even though it
exists upstream, because Magnetics' 15-core quota was entirely consumed by
Kool Mu/Edge/XFlux (a different powder family) before MPP was reached. The
cap is now tracked per (vendor, shape) pair instead, so one shape can't
starve another from the same vendor.
"""

import csv
import math
import sys
from pathlib import Path

import PyOpenMagnetics

# Reuses the exact same filtering/mapping logic as the app's loader used to
# use when it queried PyOpenMagnetics live. Kept here, not in
# python/services/magnetics_data.py, since the app no longer needs to know
# how to talk to PyOpenMagnetics at all — only how to read the CSV output.

MU0 = 4 * math.pi * 1e-7

ALLOWED_MATERIAL_TYPES = {"ferrite", "powder"}
INCLUDE_GAPPED_CORES = False
MIN_EFFECTIVE_AREA_MM2 = 20.0
MAX_EFFECTIVE_AREA_MM2 = 3000.0
MAX_CORES_PER_MATERIAL = 4
# Capped per (vendor, shape) pair, not per vendor alone - see module
# docstring for why a flat per-vendor cap silently excluded MPP toroids.
MAX_CORES_PER_VENDOR_SHAPE = 20
MAX_CORES_TO_LOAD = 220

# Specific manufacturer references that must be included whenever they
# exist upstream and pass the real material-type/application/gapping/area
# filters below, regardless of the per-material-name cap. Without this, a
# real qualifying part can still lose out to an arbitrary earlier same-name
# sibling purely because of upstream iteration order (discovered from a
# real user-submitted reference design that used Magnetics C055439A2, an
# MPP-60 toroid - one of 50 different physical sizes PyOpenMagnetics
# carries under that exact material name, only 4 of which fit under
# MAX_CORES_PER_MATERIAL). Add real reference-design part numbers here as
# they come up - never used to insert a part that doesn't actually exist
# upstream.
PRIORITY_CORE_REFERENCES = {"C055439A2X2"}


def _core_shape_and_family(functional_description: dict) -> tuple[str, str]:
    """Real shape classification from PyOpenMagnetics' own geometry record -
    never inferred from the part number or guessed. Returns ("", "") only
    when upstream genuinely has no shape data for this core."""
    shape = functional_description.get("shape") or {}
    circuit = shape.get("magneticCircuit")
    if circuit == "closed":
        core_shape = "Toroid"
    elif circuit == "open":
        core_shape = "TwoPieceSet"
    else:
        core_shape = ""
    family = shape.get("family")
    shape_family = family.upper() if family else ""
    return core_shape, shape_family


def _pick_representative_permeability(material: dict) -> float:
    raw_points = (material.get("permeability") or {}).get("initial") or []

    points = [raw_points] if isinstance(raw_points, dict) else raw_points

    if not points:
        return 0.0

    temps = sorted(
        {
            p["temperature"]
            for p in points
            if p.get("temperature") is not None
        }
    )

    if not temps:
        return points[0].get("value", 0.0)

    closest_temp = min(temps, key=lambda t: abs(t - 25.0))

    at_temp = [
        p
        for p in points
        if p.get("temperature") == closest_temp
    ]

    at_temp.sort(key=lambda p: p.get("frequency") or 0)

    return at_temp[0].get("value", 0.0)


def _core_al_nh(
    effective_area_m2: float,
    effective_length_m2: float,
    mu_r: float,
) -> float:
    if effective_length_m2 <= 0:
        return 0.0

    return (
        MU0
        * mu_r
        * effective_area_m2
        / effective_length_m2
    ) * 1e9


def _pick_saturation_flux_density_t(material: dict) -> float:
    """Real saturation flux density (Bsat) in tesla, at the temperature
    point closest to 25C - mirrors _pick_representative_permeability()'s
    same closest-to-25C selection policy. Returns 0.0 (not_evaluated,
    matching the rest of this project's honesty rule) if PyOpenMagnetics
    has no saturation data for this material at all."""
    points = material.get("saturation") or []
    if not points:
        return 0.0

    closest = min(
        points,
        key=lambda p: abs((p.get("temperature") if p.get("temperature") is not None else 25.0) - 25.0),
    )
    return closest.get("magneticFluxDensity") or 0.0


def _steinmetz_ranges(material: dict) -> list[dict]:
    """Real Steinmetz core-loss coefficients (k, alpha, beta, and the
    temperature-correction terms ct0/ct1/ct2), one entry per frequency
    range PyOpenMagnetics carries for this material. Returns an empty list
    for materials with no Steinmetz characterization upstream (the powder
    families - Kool Mu, XFlux, Edge - are characterized differently, not
    missing due to an export bug) - callers must not invent one."""
    loss_entries = (material.get("volumetricLosses") or {}).get("default") or []
    steinmetz = next((e for e in loss_entries if e.get("method") == "steinmetz"), None)
    if not steinmetz:
        return []
    return steinmetz.get("ranges") or []


def _core_mlt_mm(central_column: dict) -> float:
    """Mean-length-per-turn estimate, in mm, from the core's real central
    column cross-section - see the module docstring for what this does and
    does not account for. width/depth from PyOpenMagnetics are in meters."""
    width_m = central_column.get("width") or 0.0
    depth_m = central_column.get("depth") or 0.0
    if central_column.get("shape") == "round":
        return math.pi * width_m * 1000.0
    return 2.0 * (width_m + depth_m) * 1000.0


def fetch_materials(available_core_material_names=None,) -> list:
    PyOpenMagnetics.load_databases({})
    raw_materials = PyOpenMagnetics.get_core_materials()

    results = []

    for m in raw_materials:
        if (
            available_core_material_names is not None
            and m.get("name")
            not in available_core_material_names
        ):
            continue

        if m.get("material") not in ALLOWED_MATERIAL_TYPES:
            continue

        if "power" not in (m.get("application") or []):
            continue

        rec = m.get("recommendations") or {}

        min_freq = rec.get("minimumFrequency")
        max_freq = rec.get("maximumFrequency")

        if min_freq is None or max_freq is None:
            continue

        mat_mfr = m.get("manufacturerInfo") or {}

        results.append(
            {
                "Name": m.get("name", "Unknown"),
                "MuOpt": _pick_representative_permeability(m),
                "MinFrequencyHz": min_freq,
                "MaxFrequencyHz": max_freq,
                "Reason": (
                    f"{m.get('material', 'unknown')} material, "
                    f"family {m.get('family', '?')} "
                    f"(source: "
                    f"{mat_mfr.get('name', 'unknown')})"
                ),
                "Alternatives": "None",
                "BmaxT": _pick_saturation_flux_density_t(m),
                "MaterialType": m.get("material", ""),
                "Manufacturer": mat_mfr.get("name", ""),
                "DatasheetUrl": mat_mfr.get("datasheetUrl", "") or "",
                "SteinmetzRanges": _steinmetz_ranges(m),
            }
        )

    return results


def _is_priority_reference(core: dict) -> bool:
    mfr = core.get("manufacturerInfo") or {}
    ref = mfr.get("reference") or core.get("name")
    return ref in PRIORITY_CORE_REFERENCES


def fetch_cores() -> list[dict]:
    PyOpenMagnetics.load_databases({})
    PyOpenMagnetics.load_cores(None, True, False)
    raw_cores = PyOpenMagnetics.get_available_cores()

    # Priority references go through the filters/caps first so they win
    # their slot instead of losing to an arbitrary same-name sibling that
    # happens to come first in PyOpenMagnetics' own iteration order - see
    # PRIORITY_CORE_REFERENCES above. Everything else keeps its original
    # relative order.
    raw_cores = sorted(raw_cores, key=lambda c: 0 if _is_priority_reference(c) else 1)

    results = []

    per_material_count: dict = {}
    per_vendor_shape_count: dict = {}

    for c in raw_cores:
        fd = c.get("functionalDescription") or {}
        pd = c.get("processedDescription") or {}
        mat = fd.get("material") or {}

        if mat.get("material") not in ALLOWED_MATERIAL_TYPES:
            continue

        if "power" not in (mat.get("application") or []):
            continue

        gaps = fd.get("gapping") or []

        is_ungapped = (
            all(g.get("type") == "residual" for g in gaps)
            if gaps
            else True
        )

        if not INCLUDE_GAPPED_CORES and not is_ungapped:
            continue

        eff = pd.get("effectiveParameters") or {}

        ae_m2 = eff.get("effectiveArea")
        le_m = eff.get("effectiveLength")

        windows = pd.get("windingWindows") or []
        wa_m2 = windows[0].get("area") if windows else None

        if not ae_m2 or not le_m or not wa_m2:
            continue

        ae_mm2 = ae_m2 * 1e6
        wa_mm2 = wa_m2 * 1e6
        le_mm = le_m * 1e3

        if not (
            MIN_EFFECTIVE_AREA_MM2
            <= ae_mm2
            <= MAX_EFFECTIVE_AREA_MM2
        ):
            continue

        material_name = mat.get("name", "Unknown")

        if (
            per_material_count.get(material_name, 0)
            >= MAX_CORES_PER_MATERIAL
        ):
            continue

        mfr = c.get("manufacturerInfo") or {}
        vendor_name = mfr.get("name", "Unknown")

        core_shape, shape_family = _core_shape_and_family(fd)

        vendor_shape_key = (vendor_name, core_shape)
        if (
            per_vendor_shape_count.get(vendor_shape_key, 0)
            >= MAX_CORES_PER_VENDOR_SHAPE
        ):
            continue

        mu_r = _pick_representative_permeability(mat)

        al_nh = _core_al_nh(
            ae_m2,
            le_m,
            mu_r,
        )

        part_number = (
            mfr.get("reference")
            or c.get("name", "Unknown")
        )

        rec_freq = mat.get("recommendations") or {}

        columns = pd.get("columns") or []
        central_column = next(
            (col for col in columns if col.get("type") == "central"), None
        )
        mlt_mm = _core_mlt_mm(central_column) if central_column else 0.0

        # Real rectangular winding-window width/height (two-piece cores only -
        # PyOpenMagnetics parameterizes a toroid's window by radialHeight
        # instead, since it has no flat width/height). "" (not 0.0) means no
        # such dimension exists for this core's shape - never a real zero.
        window0 = windows[0] if windows else {}
        window_width_mm = window0.get("width")
        window_height_mm = window0.get("height")
        window_width_mm = window_width_mm * 1000.0 if window_width_mm is not None else ""
        window_height_mm = window_height_mm * 1000.0 if window_height_mm is not None else ""

        results.append(
            {
                "PartNumber": part_number,
                "Material": material_name,
                "Mu": mu_r,
                "AL": al_nh,
                "Ae": ae_mm2,
                "Wa": wa_mm2,
                "Le": le_mm,
                "Mlt": mlt_mm,
                "PartCost": 0.0,
                "Vendor": vendor_name,
                "MaxCurrent_A": 0.0,
                "MaxFreq_kHz": (
                    rec_freq.get("maximumFrequency") or 0.0
                )
                / 1000.0,
                "CoreShape": core_shape,
                "ShapeFamily": shape_family,
                "MaterialType": mat.get("material", ""),
                "DatasheetUrl": mfr.get("datasheetUrl", "") or "",
                "WindowWidthMm": window_width_mm,
                "WindowHeightMm": window_height_mm,
            }
        )

        per_material_count[material_name] = (
            per_material_count.get(material_name, 0) + 1
        )

        per_vendor_shape_count[vendor_shape_key] = (
            per_vendor_shape_count.get(vendor_shape_key, 0) + 1
        )

        if len(results) >= MAX_CORES_TO_LOAD:
            break

    return results


def main():
    data_dir = Path(__file__).resolve().parents[1] / "data"
    data_dir.mkdir(exist_ok=True)

    cores = fetch_cores()

    used_material_names = {
        c["Material"]
        for c in cores
    }

    materials = fetch_materials(
        available_core_material_names=used_material_names
    )

    if not materials or not cores:
        print(
            f"ERROR: got {len(materials)} materials, "
            f"{len(cores)} cores — refusing to overwrite "
            f"existing snapshot with empty data.",
            file=sys.stderr,
        )
        sys.exit(1)

    with open(
        data_dir / "real_materials.csv",
        "w",
        newline="",
    ) as f:
        w = csv.writer(f)

        w.writerow(
            [
                "Name",
                "MuOpt",
                "MinFrequencyHz",
                "MaxFrequencyHz",
                "Reason",
                "Alternatives",
                "BmaxT",
                "MaterialType",
                "Manufacturer",
                "DatasheetUrl",
            ]
        )

        for m in materials:
            w.writerow(
                [
                    m["Name"],
                    m["MuOpt"],
                    m["MinFrequencyHz"],
                    m["MaxFrequencyHz"],
                    m["Reason"],
                    m["Alternatives"],
                    m["BmaxT"],
                    m["MaterialType"],
                    m["Manufacturer"],
                    m["DatasheetUrl"],
                ]
            )

    with open(
        data_dir / "real_cores.csv",
        "w",
        newline="",
    ) as f:
        w = csv.writer(f)

        w.writerow(
            [
                "PartNumber",
                "Material",
                "Mu",
                "AL",
                "Ae",
                "Wa",
                "Le",
                "Mlt",
                "PartCost",
                "Vendor",
                "MaxCurrent_A",
                "MaxFreq_kHz",
                "CoreShape",
                "ShapeFamily",
                "MaterialType",
                "DatasheetUrl",
                "WindowWidthMm",
                "WindowHeightMm",
            ]
        )

        for c in cores:
            w.writerow(
                [
                    c["PartNumber"],
                    c["Material"],
                    c["Mu"],
                    c["AL"],
                    c["Ae"],
                    c["Wa"],
                    c["Le"],
                    c["Mlt"],
                    c["PartCost"],
                    c["Vendor"],
                    c["MaxCurrent_A"],
                    c["MaxFreq_kHz"],
                    c["CoreShape"],
                    c["ShapeFamily"],
                    c["MaterialType"],
                    c["DatasheetUrl"],
                    c["WindowWidthMm"],
                    c["WindowHeightMm"],
                ]
            )

    with open(
        data_dir / "real_core_loss_coefficients.csv",
        "w",
        newline="",
    ) as f:
        w = csv.writer(f)

        w.writerow(
            [
                "MaterialName",
                "MinFrequencyHz",
                "MaxFrequencyHz",
                "K",
                "Alpha",
                "Beta",
                "Ct0",
                "Ct1",
                "Ct2",
            ]
        )

        n_coefficient_rows = 0
        for m in materials:
            for r in m["SteinmetzRanges"]:
                w.writerow(
                    [
                        m["Name"],
                        r.get("minimumFrequency", 0.0),
                        r.get("maximumFrequency", 0.0),
                        r.get("k", 0.0),
                        r.get("alpha", 0.0),
                        r.get("beta", 0.0),
                        r.get("ct0", 0.0),
                        r.get("ct1", 0.0),
                        r.get("ct2", 0.0),
                    ]
                )
                n_coefficient_rows += 1

    print(
        f"Wrote {len(materials)} materials, "
        f"{len(cores)} cores, "
        f"{n_coefficient_rows} core-loss coefficient rows to {data_dir}"
    )


if __name__ == "__main__":
    main()