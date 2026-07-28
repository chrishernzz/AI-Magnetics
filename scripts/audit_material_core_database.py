"""
Read-only audit of data/real_cores.csv, data/real_materials.csv, and
data/real_core_loss_coefficients.csv - reports missing/zero/placeholder values
per row so a human can see exactly what data gaps exist in the current snapshot,
without guessing or extrapolating any of it (spec section 3).

This is a standalone reporting utility, not a runtime gate - it does not block
any request the engine actually serves; making missing-data audit block requests
would be a scope change beyond "add a reporting utility" (see the project's
Honesty Ledger). Run manually:

    python3 scripts/audit_material_core_database.py

Exits 0 always (a report, not a pass/fail check) - pipe to a file or grep if you
want to script around specific findings.
"""
import csv
from pathlib import Path

DATA_DIR = Path(__file__).resolve().parent.parent / "data"
CORES_FILE = DATA_DIR / "real_cores.csv"
MATERIALS_FILE = DATA_DIR / "real_materials.csv"
COEFFICIENTS_FILE = DATA_DIR / "real_core_loss_coefficients.csv"


def _is_missing(value: str) -> bool:
    return value is None or value.strip() == "" or value.strip().lower() in ("none", "nan")


def _is_zero(value: str) -> bool:
    try:
        return float(value) == 0.0
    except (TypeError, ValueError):
        return False


def _load_rows(path: Path) -> list[dict]:
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def audit_cores(cores: list[dict]) -> None:
    print(f"\n=== Cores ({len(cores)} rows, {CORES_FILE.name}) ===")
    # Real, load-bearing geometry/magnetic fields the engine actually reads (CoreDatabase.h).
    required_fields = ["Ae", "Wa", "Le", "Mlt", "AL", "Mu"]
    missing_by_field = {field: [] for field in required_fields}
    no_vendor = []

    for row in cores:
        part = row.get("PartNumber", "<no part number>")
        for field in required_fields:
            value = row.get(field, "")
            if _is_missing(value) or _is_zero(value):
                missing_by_field[field].append(part)
        if _is_missing(row.get("Vendor", "")):
            no_vendor.append(part)

    for field, parts in missing_by_field.items():
        if parts:
            print(f"  {field}: missing/zero for {len(parts)}/{len(cores)} cores - {', '.join(parts[:10])}{' ...' if len(parts) > 10 else ''}")
        else:
            print(f"  {field}: present and nonzero for all {len(cores)} cores")

    if no_vendor:
        print(f"  Vendor: missing for {len(no_vendor)}/{len(cores)} cores")
    else:
        print(f"  Vendor: present for all {len(cores)} cores (real manufacturer data, threaded through to CoreCandidate.vendor)")

    print("  Datasheet revision/URL/date-accessed: no such columns exist in this CSV at all - always"
          " std::nullopt in SourceInfo (see core/model/Provenance.h). Not a per-row gap to enumerate, a"
          " structural one: this data was never sourced for any core in this snapshot.")


def audit_materials(materials: list[dict]) -> None:
    print(f"\n=== Materials ({len(materials)} rows, {MATERIALS_FILE.name}) ===")
    required_fields = ["MuOpt", "MinFrequencyHz", "MaxFrequencyHz", "BmaxT"]
    missing_by_field = {field: [] for field in required_fields}

    for row in materials:
        name = row.get("Name", "<no name>")
        for field in required_fields:
            value = row.get(field, "")
            if _is_missing(value) or _is_zero(value):
                missing_by_field[field].append(name)

    for field, names in missing_by_field.items():
        if names:
            print(f"  {field}: missing/zero for {len(names)}/{len(materials)} materials - {', '.join(names[:10])}{' ...' if len(names) > 10 else ''}")
        else:
            print(f"  {field}: present and nonzero for all {len(materials)} materials")

    print("  No manufacturer/datasheet-source column exists in this CSV at all - MaterialCandidate.source"
          " never populates a manufacturer for materials (unlike cores, which do have a real Vendor column).")


def audit_core_loss_coefficients(materials: list[dict], coefficients: list[dict]) -> None:
    print(f"\n=== Core-loss coefficients ({len(coefficients)} rows, {COEFFICIENTS_FILE.name}) ===")
    material_names = {row.get("Name", "") for row in materials}
    covered = {row.get("MaterialName", "") for row in coefficients}
    uncovered = sorted(material_names - covered)

    if uncovered:
        print(f"  Materials with NO Steinmetz coefficient row at all: {len(uncovered)}/{len(material_names)} - {', '.join(uncovered)}")
        print("  Core loss will report not_evaluated for every candidate using one of these materials"
              " (see LossEvaluation.cpp's findCoreLossCoefficients call), regardless of whether ripple"
              " current was supplied - never silently approximated.")
    else:
        print(f"  All {len(material_names)} materials have at least one coefficient row covering some frequency range.")

    print("  minFluxSwingT/maxFluxSwingT/testTemperatureC: no such columns exist in this CSV - always"
          " std::nullopt in CoreLossCoefficientData (see data/CoreLossCoefficientDatabase.h). The flux-swing"
          " valid-range guard in CoreLoss.cpp is real but currently a documented no-op against this data.")


def main() -> None:
    cores = _load_rows(CORES_FILE)
    materials = _load_rows(MATERIALS_FILE)
    coefficients = _load_rows(COEFFICIENTS_FILE)

    print("AI-Magnetics material/core database audit")
    print("Read-only report - does not modify any data, does not gate the running engine.")

    audit_cores(cores)
    audit_materials(materials)
    audit_core_loss_coefficients(materials, coefficients)

    print("\nDone.")


if __name__ == "__main__":
    main()
