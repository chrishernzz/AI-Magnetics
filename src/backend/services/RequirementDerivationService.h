#pragma once
#include "../../core/InductorDesignRequest.h"
#include "../../core/InductorRequirements.h"
#include "../../rules/DesignRules.h"

// Converts the raw, unit-labeled API request into the normalized internal
// InductorRequirements every downstream stage consumes. This is the one
// place unit conversion (uH->H, kHz->Hz) and RMS-current derivation happen
// (spec section 5) - nothing downstream re-derives or re-converts.
class RequirementDerivationService {
public:
    // Throws std::invalid_argument if rmsCurrentA cannot be determined
    // (neither supplied directly nor derivable from averageCurrentA +
    // rippleCurrentPeakToPeakA).
    static InductorRequirements derive(const InductorDesignRequest& request, const DesignRules& rules);
};
