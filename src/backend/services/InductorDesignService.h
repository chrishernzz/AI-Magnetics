#pragma once
#include "../../core/DesignRecommendation.h"
#include "../../core/InductorDesignRequest.h"

// Top-level orchestrator (spec section 4): validate request -> find
// material candidates -> energy/area product -> find core candidates ->
// turns and gap -> magnetic validation -> winding -> losses -> thermal ->
// rank passing candidates -> build the explainable DesignRecommendation.
// This is the single call graph behind POST /inductor-design, replacing
// the legacy pattern where each endpoint re-ran every upstream stage.
class InductorDesignService {
public:
    // Throws std::invalid_argument if the request cannot be normalized
    // (see RequirementDerivationService).
    static DesignRecommendation run(const InductorDesignRequest& request);
};
