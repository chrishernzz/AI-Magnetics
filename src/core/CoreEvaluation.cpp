#include "CoreEvaluation.h"
#include "../data/CoreDatabase.h"
#include <algorithm>

//precondition: Ap formula converts catalog core dimensions (mm^2 x mm^2) to cm^4
//postcondition: returns the area product in cm^4 for a core with Ae and Wa in mm^2
static double coreAreaProductCm4(double aeMm2, double waMm2) {
    return (aeMm2 * waMm2) * 1e-4;
}

//precondition: CoreDatabase::load() has been populated
//postcondition: returns every core whose material matches a compatible material candidate, each flagged with whether it meets the area-product requirement (5% margin, matching the original single-pick behavior)
std::vector<CoreCandidate> findSuitableCores(const std::vector<MaterialCandidate>& compatibleMaterials, double requiredAreaProductCm4) {
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

        CoreCandidate candidate;
        candidate.partNumber = core.partNumber;
        candidate.material = core.material;
        candidate.mu = core.mu;
        candidate.al = core.al;
        candidate.aeMm2 = core.ae;
        candidate.waMm2 = core.wa;
        candidate.leMm = core.le;
        candidate.areaProductCm4 = apCm4;
        //this will return true or false based on the checking of apCm4 > requriedAreaProdcutCm4
        candidate.meetsAreaProduct = apCm4 >= requiredAreaProductCm4 * 0.95;

        candidates.push_back(std::move(candidate));
    }

    return candidates;
}
