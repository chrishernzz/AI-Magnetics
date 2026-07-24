#pragma once
#include "core/model/TopologyInput.h"
#include "core/model/InductorDesignRequest.h"

/*

Converts Buck converter operating requirements into the same
InductorDesignRequest struct Mode 2 accepts directly. Downstream of this
call, Mode 1 and Mode 2 are indistinguishable - RequirementDerivationService,
candidate generation, validation, winding, and loss evaluation all run
unchanged regardless of which mode produced the request.

Sizes inductance and ripple current at vinMaxV (see TopologyInput) as the
single worst-case operating point for V1. Evaluating ripple/peak current
across the full Vin_min..Vin_max range (and picking the worst case per
quantity, since it need not be the same point for all of them) is real,
correct practice but out of scope for this pass - documented here rather
than silently assumed away.

*/
class BuckElectricalSolver {
public:
    //throws std::invalid_argument if input.topology != Topology::Buck, or if the electrical inputs are not physically valid (vinMaxV <= voutV, any of vinMinV/vinMaxV/ioutA/switchingFreqKHz/rippleCurrentPercent
    //not positive, or vinMinV > vinMaxV) - never divides by zero or returns a negative/NaN inductance silently.
    static InductorDesignRequest solve(const TopologyInput& input);
};
