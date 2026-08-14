#pragma once
#include <optional>

/*

MODE 1: Converter-level requirements, as received from the API when the
engineer knows their converter's operating point but not the inductor's.
A TopologyElectricalSolver (BuckElectricalSolver today) converts this into
an InductorDesignRequest - the exact same struct Mode 2 accepts directly -
so nothing downstream of that point knows or cares which mode produced it.

V1 supports Buck only; fields below are Buck's. Adding a second topology
is the point where this struct would need to become topology-specific
(a variant, or one struct per topology) - not attempted now since there
is only one implementation to generalize from.

*/

enum class Topology {
    Buck
};


struct TopologyInput {
    Topology topology = Topology::Buck;

    double vinMinV;
    double vinMaxV;
    double voutV;
    double ioutA;
    double switchingFreqKHz;

    /*
    Target inductor ripple current as a percentage of ioutA (e.g. 20 for
    20%). Sizing uses vinMaxV as the worst-case point - buck ripple
    current increases with Vin, so sizing L at Vin_max keeps ripple at
    or under this target across the whole Vin range.
    */
    double rippleCurrentPercent;

    double ambientTemperatureC;
    double allowableTempRiseC;
    std::optional<double> inductanceTolerancePercent;
};
