#pragma once
#include <string>
#include <vector>
#include "MaterialEvaluation.h"

// STAGE 2: Core candidate evaluation
//
// Replaces the old selectCore() single pick (CoreSelection.h/.cpp, kept for
// the legacy /core-selection endpoint adapter) with a candidate list per
// core-compatible material, per spec section 8. No core is labeled
// "recommended" here - that only happens after turns/gap, magnetic
// validation, winding, and loss checks have all run (InductorDesignService).
// Critically: this module never falls back to "largest available core" when
// nothing meets the area-product requirement - see findSuitableCores below.

struct CoreCandidate {
    std::string partNumber;
    std::string material;
    double mu;
    double al;        // nH/turn^2, ungapped catalog value
    double aeMm2;
    double waMm2;
    double leMm;
    double areaProductCm4;
    bool meetsAreaProduct;
};

// precondition: CoreDatabase::load() has been populated
// postcondition: returns every core whose material matches one of the given
// compatible materials, each carrying its own computed area product and
// whether it meets requiredAreaProductCm4 (with a 5% margin). Cores that do
// NOT meet the requirement are still returned (meetsAreaProduct=false) so
// callers can build an honest no_feasible_design report (largest available
// Ap) instead of a silent oversized fallback - it is the caller's
// responsibility to reject the design, not this function's, to keep the
// "why did this fail" data available.
std::vector<CoreCandidate> findSuitableCores(const std::vector<MaterialCandidate>& compatibleMaterials,
                                              double requiredAreaProductCm4);
