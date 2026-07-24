#include "core/winding/WindingDesign.h"
#include "data/AwgTable.h"
#include "core/units/UnitConversions.h"
#include <algorithm>
#include <cmath>

namespace {
/*Annealed copper resistivity at 20C, ohm-meters (IACS standard value).
DCR computed here is a 20C figure, not corrected for operating
temperature - a documented Phase 1 simplification, same policy as the
gap formula's fringing-flux omission (see FORMULAS.md).*/
constexpr double kCopperResistivityOhmMAt20C = 1.724e-8;

const AwgEntry* findEntry(int awg) {
    for (const auto& entry : kAwgTable) {
        if (entry.awg == awg) {
            return &entry;
        }
    }
    return nullptr;
}

//precondition: none
//postcondition: returns the finest (highest-numbered, smallest-area) AWG entry whose cross-section area still meets requiredAreaMm2, or nullptr if even the thickest entry in the table (AWG 8) isn't enough.
const AwgEntry* finestAwgMeetingArea(double requiredAreaMm2) {
    const AwgEntry* best = nullptr;
    for (const auto& entry : kAwgTable) {
        if (entry.areaMm2 >= requiredAreaMm2) {
            if (best == nullptr || entry.awg > best->awg) {
                best = &entry;
            }
        }
    }
    return best;
}

}  // namespace

//precondition: core.waMm2 > 0, turns > 0, rmsCurrentA > 0
//postcondition: see header
WindingDesignResult designWinding(const CoreCandidate& core, int turns, double rmsCurrentA, const DesignRules& rules) {
    WindingDesignResult result;

    //A/cm^2 -> mm^2 required area: requiredAreaMm2 = I / J_Acm2, converted cm^2 -> mm^2
    double requiredAreaMm2 = units::cm2ToMm2(rmsCurrentA / rules.allowableCurrentDensityAperCm2);

    const AwgEntry* singleStrand = finestAwgMeetingArea(requiredAreaMm2);

    if (singleStrand == nullptr || singleStrand->awg < rules.minimumSingleStrandAwg) {
        // Either no single strand in the table is large enough, or the
        // implied gauge is thicker than practical for hand-winding -
        // switch to parallel strands of the minimum practical single-strand
        // gauge instead of one very thick solid wire.
        const AwgEntry* strandGauge = findEntry(rules.minimumSingleStrandAwg);
        if (strandGauge == nullptr) {
            strandGauge = &kAwgTable[0];  // fallback to the thickest table entry
        }

        int strands = std::max(1, static_cast<int>(std::ceil(requiredAreaMm2 / strandGauge->areaMm2)));

        result.wireDescription = std::to_string(strands) + "x AWG" + std::to_string(strandGauge->awg);
        result.conductorAreaMm2 = strandGauge->areaMm2;
        result.parallelStrands = strands;
    } else {
        result.wireDescription = "AWG" + std::to_string(singleStrand->awg) + " single strand";
        result.conductorAreaMm2 = singleStrand->areaMm2;
        result.parallelStrands = 1;
    }

    double totalCopperAreaPerTurnMm2 = result.conductorAreaMm2 * result.parallelStrands;

    result.currentDensityAperMm2 = rmsCurrentA / totalCopperAreaPerTurnMm2;
    result.fillFactor = (static_cast<double>(turns) * totalCopperAreaPerTurnMm2) / core.waMm2;
    result.fitsWindow = result.fillFactor <= rules.maximumFillFactor;

    if (core.mltMm > 0.0) {
        // Length of one strand's path all the way around the core, turns
        // times over - not yet divided by parallel strands.
        double singleStrandLengthM = units::mmToM(static_cast<double>(turns) * core.mltMm);
        double conductorAreaM2 = units::mm2ToM2(result.conductorAreaMm2);
        double singleStrandResistanceOhms =
            kCopperResistivityOhmMAt20C * singleStrandLengthM / conductorAreaM2;

        result.totalWireLengthM = singleStrandLengthM * result.parallelStrands;
        // Paralleling N identical strands divides resistance by N.
        result.dcrOhms = singleStrandResistanceOhms / result.parallelStrands;
        result.resistanceStatus = EvaluationStatus::Evaluated;
    } else {
        result.resistanceStatus = EvaluationStatus::NotEvaluated;
        result.missingData.push_back(
            "core '" + core.partNumber +
            "' has no mean-length-per-turn estimate available - cannot compute total wire length or DCR");
    }

    return result;
}
