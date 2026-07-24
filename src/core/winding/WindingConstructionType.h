#pragma once

// Explicit winding construction type the physical model was computed for (spec section 5's ask for
// explicit construction types instead of an implied single style). Only SingleRoundWire and
// ParallelRoundWires have a real formula in designWinding() today - Foil/Busbar/PCB are forward-looking
// schema placeholders with no formula and no request-side field to select them; they exist so the schema
// can grow later without a breaking enum change, not as a claim of present support.
enum class WindingConstructionType {
    SingleRoundWire,
    ParallelRoundWires,
    Foil,
    Busbar,
    PCB,
};

//precondition: none
//postcondition: true only for the two construction types designWinding() actually implements.
inline bool windingConstructionTypeImplemented(WindingConstructionType type) {
    return type == WindingConstructionType::SingleRoundWire || type == WindingConstructionType::ParallelRoundWires;
}
