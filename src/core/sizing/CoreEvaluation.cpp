#include "core/sizing/CoreEvaluation.h"
#include "data/CoreDatabase.h"
#include <algorithm>
#include <optional>

//precondition: Ap formula converts catalog core dimensions (mm^2 x mm^2) to cm^4
//postcondition: returns the area product in cm^4 for a core with Ae and Wa in mm^2
static double coreAreaProductCm4(double aeMm2, double waMm2) {
    return (aeMm2 * waMm2) * 1e-4;
}


//precondition: CoreDatabase::load() has been populated; requiredAreaProductCm4ByMaterial has one entry for every MaterialCandidate.materialFamily present in compatibleMaterials (the caller builds this from the same list, so every core that finds a materialCompatible match is guaranteed a lookup hit)
//precondition: returns every core whose material matches one of the given compatible materials, each carrying its own computed area product and whether it meets the materials own requiredAreaProductCm4 (with a 5% margin) - looked up per-core by material, not a single value applied to every material alike (a flat ferrite-typical flux limit was
//silently under-crediting higher-Bsat powder cores like MPP/Kool Mu, which can legitimately need less area product than the same request would demand of ferrite - see AreaProduct.h caller in InductorDesignService.cpp for how each materials own applicableFluxLimit() now feeds this map). Cores that do not meet the requirment are still returned (meetsAreaProduct=false)
//so callers can build an honest no_feasible_desing report (largest available Ap) instead of a silent oversized fallback - it is the callers responsibilty to reject teh deisng, not this functions to keep teh "why did this fail" data available
std::vector<CoreCandidate> findSuitableCores(const std::vector<MaterialCandidate>& compatibleMaterials, const std::unordered_map<std::string, double>& requiredAreaProductCm4ByMaterial) {
    const auto& cores = CoreDatabase::load();
    std::vector<CoreCandidate> candidates;

    for (const auto& core : cores) {
        bool materialCompatible = false;

        //loop through the compatible materials
        for(const auto& material: compatibleMaterials) {
            //check if any recommended material is compatible with the core's material, if not continue to the next core
            if(material.materialFamily == core.material) {
                materialCompatible = true;
                break;
            }
        }

        if(!materialCompatible) {
            continue;
        }

        double apCm4 = coreAreaProductCm4(core.ae, core.wa);

        //this core's own material requiredAp - never the same flat number for every material (a ferrite-typical flux limit was silently under-crediting higher-Bsat powder cores). The lookup is guaranteed to hit: materialCompatible above only became
        //true because some entry in compatibleMaterials has materialFamily == core.material, and the caller builds this map from that exact same list, one entry per material. Down below we are calling the strings (key) values which is a double
        double requiredAreaProductCm4 = requiredAreaProductCm4ByMaterial.at(core.material);


        CoreCandidate candidate;
        candidate.partNumber = core.partNumber;
        candidate.material = core.material;
        candidate.mu = core.mu;
        candidate.al = core.al;
        candidate.aeMm2 = core.ae;
        candidate.waMm2 = core.wa;
        candidate.leMm = core.le;
        candidate.mltMm = core.mlt;
        candidate.areaProductCm4 = apCm4;
        //The Ap requirement above (calculateAp(), McLyman's Ap=2E*1e4/(Ku*Bmax*J)) assumes gap is a free variable: "is this core physically big enough to hold the required turns/wire at Bmax" is the right
        //question for GAPPED FERRITE, where GapDesign.cpp can always add whatever gap is needed to hit the target L once the core is big enough. It is the wrong question for POWDER (distributed-gap) cores -
        //their real constraint isn't flux-density headroom, it's whether enough turns exist that the real DC-bias-rolled-off AL (see PermeabilityRolloff.h) still reaches the target inductance without the
        //design becoming physically absurd - a question this closed-form estimate cannot answer, since it has no notion of roll-off at all. Applying it to powder cores anyway meant most of them were being
        //excluded before TurnsAndGapDesign.cpp's real iterative solve (which DOES know about roll-off) ever got a chance to run - a real user report (61 of 80 powder cores in the DB never reaching real
        //evaluation at a supplied peak current) caught this. Same fix philosophy already used elsewhere in this function for peakSupplied==false: when a shortcut can't be trusted for a case, skip it and let
        //the real per-candidate check decide - so every powder core unconditionally meets this pre-filter and is judged only by the real turns/gap/DC-bias solve and the real DesignValidation checks downstream.
        candidate.meetsAreaProduct = (core.materialType == "powder") || (apCm4 >= requiredAreaProductCm4 * 0.95);

        //real vendor data, already fetched by magnetics_data.py but previously dropped before reaching this struct - not fabricated, just finally threaded through.
        candidate.vendor = core.vendor;
        candidate.coreShape = core.coreShape;
        candidate.shapeFamily = core.shapeFamily;
        candidate.materialType = core.materialType;
        candidate.windowWidthMm = core.windowWidthMm;
        candidate.windowHeightMm = core.windowHeightMm;
        candidate.surfaceAreaWoundMm2 = core.surfaceAreaWoundMm2;
        candidate.source.manufacturer = core.vendor.empty() ? std::nullopt : std::optional<std::string>(core.vendor);
        candidate.source.partNumber = core.partNumber;
        candidate.source.materialGrade = core.material;
        candidate.source.datasheetName = "PyOpenMagnetics/MAS export (data/real_cores.csv snapshot)";
        //real for ~99% of cores in the current snapshot - see CoreData::datasheetUrl. datasheetRevision/dateAccessed remain unset (no such data exists anywhere in the snapshot).
        candidate.source.datasheetUrl = core.datasheetUrl.empty() ? std::nullopt : std::optional<std::string>(core.datasheetUrl);
        candidate.source.confidence = core.vendor.empty() ? DataConfidence::Estimated : DataConfidence::Manufacturer;
        candidate.source.confidenceNote = core.vendor.empty() ? "no vendor recorded for this core in the snapshot" : "geometry, vendor, and datasheet URL sourced from PyOpenMagnetics/MAS, itself built from real manufacturer datasheets - datasheet revision/date-accessed are not tracked in this snapshot";
        candidates.push_back(std::move(candidate));
    }

    return candidates;
}
