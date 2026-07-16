#include "CoreEvaluation.h"
#include "../data/CoreDatabase.h"
#include <algorithm>

// Ap = Ae x Wa, mm^2 x mm^2 -> cm^4 (1 cm^4 = 1e4 mm^4)
static double coreAreaProductCm4(double aeMm2, double waMm2) {
    return (aeMm2 * waMm2) * 1e-4;
}

// precondition: CoreDatabase::load() has been populated
// postcondition: returns every core whose material matches a compatible
// material candidate, each flagged with whether it meets the area-product
// requirement (5% margin, matching the original single-pick behavior) -
// never silently substitutes an undersized core as a pass.
std::vector<CoreCandidate> findSuitableCores(const std::vector<MaterialCandidate>& compatibleMaterials,
                                              double requiredAreaProductCm4) {
    const auto& cores = CoreDatabase::load();
    std::vector<CoreCandidate> candidates;

    for (const auto& core : cores) {
        bool materialCompatible = std::any_of(
            compatibleMaterials.begin(), compatibleMaterials.end(),
            [&](const MaterialCandidate& m) { return m.materialFamily == core.material; });

        if (!materialCompatible) {
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
        candidate.meetsAreaProduct = apCm4 >= requiredAreaProductCm4 * 0.95;

        candidates.push_back(std::move(candidate));
    }

    return candidates;
}
