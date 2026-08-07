#pragma once
#include <string>
#include <vector>
#include<unordered_map>
#include "core/sizing/MaterialEvaluation.h"

/*

STAGE 2: Core candidate evaluation
This will now take the list of suitable materials from Stage 1 and the required area product from Stage 2 and 
return a list of all the cores that are compatible with the suitable materials and meet the required area product.
Each core will be returned as a CoreCandidate struct, which will contain the core's part number, material, mu, al, aeMm2, waMm2, leMm, areaProductCm4, 
and whether it meets the required area product. Note that cores that do not meet the required area product are still returned (meetsAreaProduct=false) so callers 
can build an honest no_feasible_design report (largest available Ap) instead of a silent oversized fallback - it is the caller's responsibility to reject the design, 
not this function's, to keep the "why did this fail" data available.

*/
struct CoreCandidate {
    std::string partNumber;
    std::string material;
    //Defaulted to 0.0/false (not left indeterminate) - every real construction path sets these explicitly, but a default-constructed instance (e.g. in a test fixture) would otherwise read genuinely uninitialized
    //garbage. See MaterialCandidate::hasBmaxData for the real g++-11-specific test failure this exact pattern caused elsewhere in this codebase.
    double mu = 0.0;
    //nH/turn^2, ungapped catalog value
    double al = 0.0;
    double aeMm2 = 0.0;
    double waMm2 = 0.0;
    double leMm = 0.0;
    //Mean-length-per-turn, mm. 0.0 means no data - see CoreData::mlt.
    double mltMm = 0.0;
    double areaProductCm4 = 0.0;
    bool meetsAreaProduct = false;

    //real vendor/manufacturer name from real_cores.csv's Vendor column - empty string means no vendor recorded for this core.
    std::string vendor;

    //real shape classification ("Toroid"/"TwoPieceSet") from real_cores.csv's CoreShape column - empty string means no shape data recorded for this core.
    std::string coreShape;

    //human-readable geometry family (e.g. "T", "ETD", "PQ") from real_cores.csv's ShapeFamily column - empty string means no shape data recorded.
    std::string shapeFamily;

    //real material type ("ferrite"/"powder") from real_cores.csv's MaterialType column - see CoreData::materialType. Empty string means unknown, never treated as "powder".
    std::string materialType;

    //real rectangular winding-window dimensions (mm) - see CoreData::windowWidthMm/windowHeightMm. 0.0 means no linear dimension recorded (always true for toroids - their window is a radial height, not a flat
    //width/height). Consumed by WindingDesign's BundleFitValidation check (parallel-strand bundle width vs. narrowest real opening) - the raw fill-factor formula still uses the area-fraction margin/lead-exit
    //estimates from DesignRules, unchanged.
    double windowWidthMm = 0.0;
    double windowHeightMm = 0.0;

    //real, manufacturer-published wound-coil external surface area (mm^2) - see CoreData::surfaceAreaWoundMm2. 0.0 means not yet transcribed, never a guessed/estimated value. Consumed by ThermalEvaluation as the
    //preferred (real) input to the Rth calculation, ahead of the Ae*Le compact-solid estimate.
    double surfaceAreaWoundMm2 = 0.0;

    //provenance for this core's data - see Provenance.h. source.datasheetUrl is now real (populated from CoreData::datasheetUrl) for ~99% of cores in the current snapshot; datasheetRevision/dateAccessed
    //remain unset (no such data exists anywhere in the snapshot).
    SourceInfo source;
};

std::vector<CoreCandidate> findSuitableCores(const std::vector<MaterialCandidate>& compatibleMaterials, const std::unordered_map<std::string, double>& requiredAreaProductCm4ByMaterial);
