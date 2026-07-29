#pragma once
#include <string>
#include <vector>
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
    double mu;
    // nH/turn^2, ungapped catalog value
    double al;        
    double aeMm2;
    double waMm2;
    double leMm;
    // Mean-length-per-turn, mm. 0.0 means no data - see CoreData::mlt.
    double mltMm = 0.0;
    double areaProductCm4;
    bool meetsAreaProduct;

    //real vendor/manufacturer name from real_cores.csv's Vendor column - empty string means no vendor recorded for this core.
    std::string vendor;

    //real shape classification ("Toroid"/"TwoPieceSet") from real_cores.csv's CoreShape column - empty string means no shape data recorded for this core.
    std::string coreShape;

    //human-readable geometry family (e.g. "T", "ETD", "PQ") from real_cores.csv's ShapeFamily column - empty string means no shape data recorded.
    std::string shapeFamily;

    //real material type ("ferrite"/"powder") from real_cores.csv's MaterialType column - see CoreData::materialType. Empty string means unknown, never treated as "powder".
    std::string materialType;

    //real rectangular winding-window dimensions (mm) - see CoreData::windowWidthMm/windowHeightMm. 0.0 means
    //no linear dimension recorded (always true for toroids - their window is a radial height, not a flat
    //width/height). Not yet consumed by WindingDesign's fill-factor formula, which still uses the
    //area-fraction margin/lead-exit estimates from DesignRules.
    double windowWidthMm = 0.0;
    double windowHeightMm = 0.0;

    //provenance for this core's data - see Provenance.h. source.datasheetUrl is now real (populated from
    //CoreData::datasheetUrl) for ~99% of cores in the current snapshot; datasheetRevision/dateAccessed
    //remain unset (no such data exists anywhere in the snapshot).
    SourceInfo source;
};

//precondition: CoreDatabase::load() has been populated
//postcondition: returns every core whose material matches one of the given compatible materials, each carrying its own computed area product and
/*whether it meets requiredAreaProductCm4 (with a 5% margin). Cores that do
  Not meet the requirement are still returned (meetsAreaProduct=false) so
  callers can build an honest no_feasible_design report (largest available
  Ap) instead of a silent oversized fallback - it is the caller's
  responsibility to reject the design, not this function's, to keep the
  "why did this fail" data available.*/
std::vector<CoreCandidate> findSuitableCores(const std::vector<MaterialCandidate>& compatibleMaterials, double requiredAreaProductCm4);
