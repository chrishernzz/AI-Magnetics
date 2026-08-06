#pragma once
#include <vector>
#include <string>
#include "DataCache.h"
//this is our data that core holds from the database 'cores.csv'
struct CoreData {
    std::string partNumber;
    std::string material;
    double mu;
    double al;
    double ae;
    double wa;
    double le;

    //Mean-length-per-turn (mm), estimated from real core column geometry (see scripts/export_real_data.py) - 0.0 means "no data", callers must treat that as missing, not as a real zero-length core.
    double mlt = 0.0;

    //Real vendor/manufacturer name from real_cores.csv's Vendor column - already fetched by magnetics_data.py but previously never threaded through to the C++ layer. Empty string means no vendor recorded.
    std::string vendor;

    //Real shape classification from PyOpenMagnetics' own geometry record (see scripts/export_real_data.py's _core_shape_and_family) - "Toroid" or "TwoPieceSet". Empty string means no shape data recorded for this core.
    std::string coreShape;

    //Human-readable geometry family (e.g. "T", "ETD", "PQ", "RM") from the same source as coreShape. Empty string means no shape data recorded.
    std::string shapeFamily;

    //Real material type ("ferrite" or "powder") from PyOpenMagnetics' own material.material field (see
    //scripts/export_real_data.py) - not inferred from coreShape or the part number. A toroid can be either:
    //powder toroids (MPP, Kool Mu, High Flux, Edge, XFlux, etc.) get their effective permeability from
    //particle-level distributed gapping baked into the catalog AL; ferrite toroids (e.g. N87) do not - they
    //need the same real machined-gap formula as a two-piece core. Empty string means no material-type data
    //recorded; callers must treat that as unknown, never as "powder".
    std::string materialType;

    //Real datasheet URL from real_cores.csv's DatasheetUrl column (PyOpenMagnetics' own manufacturerInfo -
    //populated for ~99% of cores in the current snapshot). Empty string means genuinely not recorded for
    //this part, never a placeholder link.
    std::string datasheetUrl;

    //Real rectangular winding-window dimensions (mm) from real_cores.csv's WindowWidthMm/WindowHeightMm
    //columns - populated for two-piece (open-shape) cores, always 0.0 for toroids (their window is
    //described by a radial height instead, not a flat width/height - not a missing-data bug). 0.0 means
    //"no linear dimension recorded", the same convention as mlt above - never a real zero-size window.
    double windowWidthMm = 0.0;
    double windowHeightMm = 0.0;

    //Real, manufacturer-published external surface area of the WOUND coil (mm^2), transcribed directly
    //from the datasheet's own "Surface Area" table (e.g. C055439A2's datasheet publishes 9,600mm^2 at a
    //40% winding factor) - not computed or estimated. A real user report (Roger, senior magnetics
    //engineer) flagged that estimating this from core volume (Ae*Le, a compact-solid shape-factor guess)
    //materially under-states a wound toroid's real cooling surface, producing an inflated thermal-rise
    //estimate. 0.0 means not yet transcribed for this part - this snapshot only has one confirmed value
    //(C055439A2) as of this field's introduction; every other row is genuinely unpopulated, never guessed.
    double surfaceAreaWoundMm2 = 0.0;
};

class CoreDatabase{
public: 
    static const std::vector<CoreData>& load() {
        return DataCache<CoreData>::load("CoreDatabase");
    }
    static void setData(std::vector<CoreData> data){
        DataCache<CoreData>::setData(std::move(data));
    }
};