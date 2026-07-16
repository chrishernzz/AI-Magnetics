#include "MaterialEvaluation.h"
#include "../data/Materials.h"

//precondition: Materials::load() has been populated
//postcondition: returns every material whose declared frequency range, contains the requested switching frequency, each as an independent candidate - no material is dropped just because another also matches.
std::vector<MaterialCandidate> findSuitableMaterials(const OperatingPoint& operatingPoint) {
    const auto& materials = Materials::load();
    std::vector<MaterialCandidate> candidates;

    for (const auto& material : materials) {
        //get a true/false to see if the material is suitable for the requested switching frequency
        bool frequencySuitable = operatingPoint.switchingFreqHz >= material.minFrequencyHz && operatingPoint.switchingFreqHz < material.maxFrequencyHz;

        //if false just continue through the data to get the other materials that are suitable for the requested switching frequency
        if (!frequencySuitable) {
            continue;
        }

        MaterialCandidate candidate;
        candidate.materialFamily = material.name;
        candidate.muOpt = material.muOpt;
        candidate.frequencySuitable = true;
        candidate.hasBmaxData = material.bmaxT > 0.0;
        candidate.hasCoreLossData = material.cuLossFactor > 0.0;
        candidate.bmaxT = material.bmaxT;
        candidate.cuLossFactor = material.cuLossFactor;
        candidate.reason = material.reason;
        candidate.alternatives = material.alternatives;

        if (!candidate.hasBmaxData) {
            candidate.missingDataWarnings.push_back(
                "no measured saturation flux density (BmaxT) for material '" + material.name +
                "' - saturation checks will use the Phase 1 default flux density limit, not a "
                "material-specific value");
        }
        if (!candidate.hasCoreLossData) {
            candidate.missingDataWarnings.push_back(
                "no core-loss coefficient (CuLossFactor) for material '" + material.name +
                "' - core loss will be reported as not evaluated");
        }

        candidates.push_back(std::move(candidate));
    }

    return candidates;
}
