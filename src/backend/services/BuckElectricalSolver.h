#pragma once
#include <string>
#include <vector>
#include "core/model/TopologyInput.h"
#include "core/model/InductorDesignRequest.h"
#include "core/model/ConductionMode.h"
#include "rules/DesignRules.h"

/*

Converts Buck converter operating requirements into the same
InductorDesignRequest struct Mode 2 accepts directly. Downstream of this
call, Mode 1 and Mode 2 are indistinguishable - RequirementDerivationService,
candidate generation, validation, winding, and loss evaluation all run
unchanged regardless of which mode produced the request.

Inductance and ripple current are still sized at vinMaxV as the single
worst-case point (buck ripple current increases with Vin), but the real
per-quantity worst case is not always the same operating point - duty
cycle is worst at Vin_min, not Vin_max. BuckSolveResult now reports both
operating points explicitly (atVinMin/atVinMax) plus which one is worst
for each quantity, instead of silently only ever showing the Vin_max
numbers.

*/

//One operating point's derived electrical quantities, evaluated at a specific Vin using the inductance value that was actually sized (at
//Vin_max) - these are NOT independently re-sized per point, they show how the one physical inductor behaves across the input range.
struct BuckOperatingPointResult {
    double vinV = 0.0;
    double dutyCycle = 0.0;
    double rippleCurrentPeakToPeakA = 0.0;
    double peakCurrentA = 0.0;
    //Iout - ripple/2 at this Vin, using the fixed sized inductance - the quantity CCM/DCM classification is based on (see conductionMode).
    double minInductorCurrentA = 0.0;
};

struct BuckSolveResult {
    //sized at the worst-case point for ripple current (Vin_max) - the exact same InductorDesignRequest Mode 2 accepts directly, so nothing downstream changes.
    InductorDesignRequest request;

    BuckOperatingPointResult atVinMin;
    BuckOperatingPointResult atVinMax;

    //"VinMin" | "VinMax" - which operating point is worst for each quantity, since it need not be the same point for all three.
    std::string worstCaseDutyCycleAt;
    std::string worstCaseRippleAt;
    std::string worstCasePeakAt;

    ConductionMode conductionMode = ConductionMode::CCM;

    //Stated Phase 1 assumptions (ideal converter, CCM, triangular ripple, no PCB/connector drop) - never silently assumed without being surfaced.
    std::vector<std::string> assumptions;

    //Non-fatal notices (e.g. CCM-boundary) - conditions worth flagging that do not by themselves make the request invalid.
    std::vector<std::string> warnings;
};

class BuckElectricalSolver {
public:
    //throws std::invalid_argument if input.topology != Topology::Buck; if any numeric field is non-finite (NaN/infinity); if the electrical inputs are not physically valid (vinMinV <= voutV - the real
    //binding constraint is Vout vs Vin_min, since duty cycle is maximum at the low end of the input range - not vinMaxV, which is only the worst case for ripple current; any of
    //vinMinV/vinMaxV/ioutA/switchingFreqKHz/rippleCurrentPercent not positive; rippleCurrentPercent above rules.maximumRippleCurrentPercent; or vinMinV > vinMaxV); or if the computed
    //minimum inductor current at Vin_max is negative, meaning the operating point is already in discontinuous conduction mode (DCM), which Phase 1's formulas do not support.
    //never divides by zero or returns a negative/NaN inductance silently.
    static BuckSolveResult solve(const TopologyInput& input, const DesignRules& rules);
};
