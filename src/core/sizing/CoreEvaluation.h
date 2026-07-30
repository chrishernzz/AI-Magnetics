#pragma once
#include <string>
#include <vector>
#include<unordered_map>
#include "core/sizing/MaterialEvaluation.h"

/*

STAGE 2: Core candidate evaluation
This will now take the list of suitable materials from Stage 1 and the required area product from Stage 2 and 
return a list of all the cores that are compatible with the suitable materials and meet the required area product.
Each core will be returned as a CoreCandidate struct, which will contain the core's part number, material, mu, al, aeMm2, waMm2, leMm, areaProductCm4, 
and whether it meets the required area product. Note that cores that do not meet the required area product are still returned (meetsAreaProduct=false) so callers 
can build an honest no_feasible_design report (largest available Ap) instead of a silent oversized fallback - it is the caller's responsibility to reject the design, 
not this function's, to keep the "why did this fail" data available.

*/
struct CoreCandidate {
    std::string partNumber;
    std::string material;
    //Defaulted to 0.0/false (not left indeterminate) - every real construction path sets these explicitly,
    //but a default-constructed instance (e.g. in a test fixture) would otherwise read genuinely uninitialized
    //garbage. See MaterialCandidate::hasBmaxData for the real g++-11-specific test failure this exact pattern
    //caused elsewhere in this codebase.
    double mu = 0.0;
    // nH/turn^2, ungapped catalog value
    double al = 0.0;
    double aeMm2 = 0.0;
    double waMm2 = 0.0;
    double leMm = 0.0;
    // Mean-length-per-turn, mm. 0.0 means no data - see CoreData::mlt.
    double mltMm = 0.0;
    double areaProductCm4 = 0.0;
    bool meetsAreaProduct = false;

    //real vendor/manufacturer name from real_cores.csv's Vendor column - empty string means no vendor recorded for this core.
    std::string vendor;

    //real shape classification ("Toroid"/"TwoPieceSet") from real_cores.csv's CoreShape column - empty string means no shape data recorded for this core.
    std::string coreShape;

    //human-readable geometry family (e.g. "T", "ETD", "PQ") from real_cores.csv's ShapeFamily column - empty string means no shape data recorded.
    std::string shapeFamily;

    //real material type ("ferrite"/"powder") from real_cores.csv's MaterialType column - see CoreData::materialType. Empty string means unknown, never treated as "powder".
    std::string materialType;

    //real rectangular winding-window dimensions (mm) - see CoreData::windowWidthMm/windowHeightMm. 0.0 means
    //no linear dimension recorded (always true for toroids - their window is a radial height, not a flat
    //width/height). Consumed by WindingDesign's BundleFitValidation check (parallel-strand bundle width vs.
    //narrowest real opening) - the raw fill-factor formula still uses the area-fraction margin/lead-exit
    //estimates from DesignRules, unchanged.
    double windowWidthMm = 0.0;
    double windowHeightMm = 0.0;

    //provenance for this core's data - see Provenance.h. source.datasheetUrl is now real (populated from
    //CoreData::datasheetUrl) for ~99% of cores in the current snapshot; datasheetRevision/dateAccessed
    //remain unset (no such data exists anywhere in the snapshot).
    SourceInfo source;
};


//CoreDatabase::load() has been populated; requiredAreaProductCm4ByMaterial has one entry for every MaterialCandidate.materialFamily
//present in compatibleMaterials (the caller builds this from the same list, so every core that finds a materialCompatible match is guaranteed a lookup hit)
//returns every core whose material matches one of the given compatible materials, each carrying its own computed area product and whether it meets the materials own
//requiredAreaProductCm4 (with a 5% margin) - looked up per-core by material, not a single value applied to every material alike (a flat ferrite-typical flux limit was
//silently under-crediting higher-Bsat powder cores like MPP/Kool Mu, which can legitimately need less area product than the same request would demand of ferrite - see AreaProduct.h
//caller in InductorDesignService.cpp for how each materials own applicableFluxLimit() now feeds this map). Cores that do not meet the requirment are still returned (meetsAreaProduct=false)
// so callers can build an honest no_feasible_desing report (largest available Ap) instead of a silent oversized fallback - it is the callers responsibilty to reject teh deisng, not this
//functions to keep teh "why did this fail" data available
std::vector<CoreCandidate> findSuitableCores(const std::vector<MaterialCandidate>& compatibleMaterials, const std::unordered_map<std::string, double>& requiredAreaProductCm4ByMaterial);
