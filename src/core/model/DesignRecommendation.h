#pragma once
#include <string>
#include <vector>
#include "core/model/InductorCandidate.h"
#include "core/model/EngineVersions.h"
#include "rules/DesignRules.h"

/*

Final explainable pipeline output (spec sections 4 and 8). status is
"ok" when at least one candidate passed every check, or
"no_feasible_design" when nothing did - the service never falls back to
an unsafe/oversized recommendation when nothing actually qualifies.

*/
struct DesignRecommendation {
    //"ok" | "no_feasible_design"
    std::string status; 
    std::string message;
    //passed, ranked best first
    std::vector<InductorCandidate> candidates;     
    //evaluated but did not pass every check    
    std::vector<InductorCandidate> rejectedCandidates;  
    DesignRules activeRules;

    //populated only when status == "no_feasible_design" and the failure is an area-product shortfall (spec section 8's structured result).
    double requiredAreaProductCm4 = 0.0;
    double largestAvailableAreaProductCm4 = 0.0;

    //reproducibility metadata for this run - see EngineVersions.h.
    EngineVersions versions;
};
