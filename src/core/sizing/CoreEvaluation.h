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

    //provenance for this core's data - see Provenance.h. datasheetRevision/Url/dateAccessed are always unset
    //in Phase 1 (no real per-core datasheet revision/URL/access-date exists in this snapshot).
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
