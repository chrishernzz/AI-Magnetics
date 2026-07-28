#include "core/sizing/MaterialEvaluation.h"
#include "data/MaterialDatabase.h"
#include "data/CoreLossCoefficientDatabase.h"

namespace {
//precondition: none
//postcondition: true if CoreLossCoefficientDatabase has at least one row for materialName, at any frequency - this is a material-level "does any real data exist" flag, separate from findCoreLossCoefficients()'s frequency-specific lookup used at loss-evaluation time.
bool hasAnyCoreLossCoefficients(const std::string& materialName) {
    for (const auto& row : CoreLossCoefficientDatabase::load()) {
        if (row.materialName == materialName) {
            return true;
        }
    }
    return false;
}
}  // namespace

//precondition: Materials::load() has been populated
//postcondition: returns every material whose declared frequency range, contains the requested switching frequency, each as an independent candidate - no material is dropped just because another also matches.
std::vector<MaterialCandidate> findSuitableMaterials(const OperatingPoint& operatingPoint) {
    const auto& materials = MaterialDatabase::load();
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
        candidate.hasCoreLossData = hasAnyCoreLossCoefficients(material.name);
        candidate.bmaxT = material.bmaxT;
        candidate.cuLossFactor = material.cuLossFactor;
        candidate.reason = material.reason;
        candidate.alternatives = material.alternatives;

        if (!candidate.hasBmaxData) {
            candidate.missingDataWarnings.push_back("no measured saturation flux density (BmaxT) for material '" + material.name + "' - saturation checks will use the Phase 1 default flux density limit, not a material-specific value");
        }
        if (!candidate.hasCoreLossData) {
            candidate.missingDataWarnings.push_back("no validated Steinmetz core-loss coefficients for material '" + material.name + "' - core loss will be reported as not evaluated regardless of ripple current");
        }

        //no manufacturer/part-number column exists for materials in real_materials.csv - only left unset,
        //never fabricated. datasheetRevision/Url/dateAccessed are always unset (no such data exists).
        candidate.source.datasheetName = "PyOpenMagnetics/MAS export (data/real_materials.csv snapshot)";
        candidate.source.confidence = (candidate.hasBmaxData && candidate.hasCoreLossData)
            ? DataConfidence::Manufacturer
            : DataConfidence::Estimated;
        candidate.source.confidenceNote = (candidate.hasBmaxData && candidate.hasCoreLossData)
            ? "saturation flux density and core-loss coefficients sourced from PyOpenMagnetics/MAS, itself built "
              "from real manufacturer datasheets - no datasheet revision/URL/date-accessed is tracked in this snapshot"
            : "one or more of saturation flux density / core-loss coefficients is missing for this material in "
              "the current snapshot - see missingDataWarnings";

        candidates.push_back(std::move(candidate));
    }

    return candidates;
}
