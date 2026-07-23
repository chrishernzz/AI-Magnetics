#pragma once

/*

Which converter topology a TopologyInput describes. Buck is the only
implemented solver today (V1 scope) - adding Boost/Flyback later means
adding one more enum value plus one more solver, not touching this file's
callers.

*/
enum class Topology {
    Buck
};
