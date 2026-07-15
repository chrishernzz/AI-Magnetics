"""
export_real_data.py

Maintenance script — NOT part of the running app. Run this to regenerate
data/real_materials.csv and data/real_cores.csv from a fresh PyOpenMagnetics
query.

Must be run somewhere PyOpenMagnetics actually installs — Linux, macOS, or
Windows via WSL2. It will not run on native Windows (see docs/ARCHITECTURE.md
-> "Data Source: Real Data, No Live Windows Dependency" for why).

Usage:
    pip install PyOpenMagnetics
    python scripts/export_real_data.py

This overwrites data/real_materials.csv and data/real_cores.csv. The running
app (python/services/magnetics_data.py) reads those files — it never imports
PyOpenMagnetics itself.
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
MAX_CORES_PER_VENDOR = 15
MAX_CORES_TO_LOAD = 60


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
                    f"{(m.get('manufacturerInfo') or {}).get('name', 'unknown')})"
                ),
                "Alternatives": "None",
                "BmaxT": 0.0,
                "CuLossFactor": 0.0,
            }
        )

    return results


def fetch_cores() -> list[dict]:
    PyOpenMagnetics.load_databases({})
    PyOpenMagnetics.load_cores(None, True, False)
    raw_cores = PyOpenMagnetics.get_available_cores()

    results = []

    per_material_count: dict = {}
    per_vendor_count: dict = {}

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

        if (
            per_vendor_count.get(vendor_name, 0)
            >= MAX_CORES_PER_VENDOR
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

        results.append(
            {
                "PartNumber": part_number,
                "Material": material_name,
                "Mu": mu_r,
                "AL": al_nh,
                "Ae": ae_mm2,
                "Wa": wa_mm2,
                "Le": le_mm,
                "PartCost": 0.0,
                "Vendor": vendor_name,
                "MaxCurrent_A": 0.0,
                "MaxFreq_kHz": (
                    rec_freq.get("maximumFrequency") or 0.0
                )
                / 1000.0,
            }
        )

        per_material_count[material_name] = (
            per_material_count.get(material_name, 0) + 1
        )

        per_vendor_count[vendor_name] = (
            per_vendor_count.get(vendor_name, 0) + 1
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
                "CuLossFactor",
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
                    m["CuLossFactor"],
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
                "PartCost",
                "Vendor",
                "MaxCurrent_A",
                "MaxFreq_kHz",
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
                    c["PartCost"],
                    c["Vendor"],
                    c["MaxCurrent_A"],
                    c["MaxFreq_kHz"],
                ]
            )

    print(
        f"Wrote {len(materials)} materials, "
        f"{len(cores)} cores to {data_dir}"
    )


if __name__ == "__main__":
    main()