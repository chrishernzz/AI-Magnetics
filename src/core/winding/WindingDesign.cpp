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

constexpr double kPi = 3.14159265358979323846;

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
    int selectedAwg = 0;

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
        selectedAwg = strandGauge->awg;
    } 
    else {
        result.wireDescription = "AWG" + std::to_string(singleStrand->awg) + " single strand";
        result.conductorAreaMm2 = singleStrand->areaMm2;
        result.parallelStrands = 1;
        selectedAwg = singleStrand->awg;
    }

    double totalCopperAreaPerTurnMm2 = result.conductorAreaMm2 * result.parallelStrands;

    result.currentDensityAperMm2 = rmsCurrentA / totalCopperAreaPerTurnMm2;
    result.effectiveCurrentDensityAperMm2 = result.parallelStrands > 1 ? result.currentDensityAperMm2 / rules.currentSharingDerateFactor : result.currentDensityAperMm2;
    result.fillFactor = (static_cast<double>(turns) * totalCopperAreaPerTurnMm2) / core.waMm2;
    result.fitsWindow = result.fillFactor <= rules.maximumFillFactor;

    result.constructionType = result.parallelStrands > 1 ? WindingConstructionType::ParallelRoundWires : WindingConstructionType::SingleRoundWire;

    // Bare-strand diameter for whichever gauge was actually selected (single or parallel-strand case).
    double bareStrandDiameterMm = 2.0 * std::sqrt(result.conductorAreaMm2 / kPi);
    result.insulatedConductorDiameterMm = bareStrandDiameterMm + rules.singleBuildInsulationBuildUpMm;
    result.insulatedConductorAreaMm2 = (kPi / 4.0) * result.insulatedConductorDiameterMm * result.insulatedConductorDiameterMm;

    if (result.parallelStrands > 1) {
        result.physicalDescription = std::to_string(result.parallelStrands) + "x AWG" + std::to_string(selectedAwg) +
            " magnet wire (bare " + std::to_string(bareStrandDiameterMm) + " mm, insulated ~" +
            std::to_string(result.insulatedConductorDiameterMm) + " mm each), wound as a single bundle";

        // Real check, only possible for two-piece cores (real window width/height - see
        // CoreCandidate::windowWidthMm/windowHeightMm). Toroids have no flat width/height at all (radial
        // window geometry instead), so they - and any two-piece core still missing this data - stay
        // permanently NotEvaluated here, never assumed to fit.
        if (core.windowWidthMm > 0.0 && core.windowHeightMm > 0.0) {
            result.bundleWidthMm = static_cast<double>(result.parallelStrands) * result.insulatedConductorDiameterMm;
            result.narrowestWindowOpeningMm = std::min(core.windowWidthMm, core.windowHeightMm);
            result.bundleFitsWindowOpening = result.bundleWidthMm <= result.narrowestWindowOpeningMm;
            result.bundleFitStatus = EvaluationStatus::Evaluated;
        } else {
            result.missingData.push_back(
                "parallel-strand bundle-vs-narrowest-opening fit is not evaluated - this core has no real "
                "window width/height (toroid, or a two-piece core missing that data in the current snapshot)");
        }
    } else {
        result.physicalDescription = "AWG" + std::to_string(selectedAwg) + " magnet wire (bare " +
            std::to_string(bareStrandDiameterMm) + " mm, insulated ~" + std::to_string(result.insulatedConductorDiameterMm) + " mm), single strand";
    }

    // Physical window fill: bobbin-wall derate, then subtract margin/lead-exit clearance (both expressed as
    // area fractions in DesignRules.h - real window width/height now exists for two-piece cores, see
    // CoreCandidate::windowWidthMm/windowHeightMm, but this formula does not yet switch to a literal-mm
    // margin for them; toroids still have no linear window dimension at all), then divide the
    // insulated-conductor area sum by the achievable packing factor to get the physically occupied area.
    result.physicalWindowAreaMm2 = core.waMm2 * rules.bobbinWindowDerateFactor * (1.0 - rules.marginAllowanceAreaFraction - rules.leadExitAllowanceAreaFraction);
    double physicalCopperAreaMm2 = (static_cast<double>(turns) * result.parallelStrands * result.insulatedConductorAreaMm2) / rules.packingFactor;
    result.physicalWindowFillFactor = result.physicalWindowAreaMm2 > 0.0 ? physicalCopperAreaMm2 / result.physicalWindowAreaMm2 : 0.0;
    result.fitsPhysicalWindow = result.physicalWindowFillFactor <= rules.maximumFillFactor;

    if (core.mltMm > 0.0) {
        // Length of one strand's path all the way around the core, turns
        // times over - not yet divided by parallel strands.
        result.coreWindingLengthM = units::mmToM(static_cast<double>(turns) * core.mltMm);
        result.leadLengthM = units::mmToM(rules.totalLeadLengthAllowanceMm);
        result.routingLengthM = units::mmToM(rules.routingLengthAllowanceMm);
        result.totalLengthM = result.coreWindingLengthM + result.leadLengthM + result.routingLengthM;

        double conductorAreaM2 = units::mm2ToM2(result.conductorAreaMm2);
        double singleStrandResistanceOhms = kCopperResistivityOhmMAt20C * result.totalLengthM / conductorAreaM2;
        result.connectionResistanceOhms = units::milliOhmToOhm(rules.connectionResistanceMilliOhm);

        result.totalWireLengthM = result.coreWindingLengthM * result.parallelStrands;
        // Paralleling N identical strands divides resistance by N; the termination joint is a single
        // connection, not itself paralleled across strands.
        result.coldDcrOhmsAt20C = singleStrandResistanceOhms / result.parallelStrands + result.connectionResistanceOhms;
        result.dcrOhms = result.coldDcrOhmsAt20C;

        // Conservative sanity-check estimate only - not a converged answer. Overwritten by the real
        // iterative thermal loop's converged hot DCR once that loop exists (see ThermalEvaluation.h).
        result.estimatedHotDcrOhms = result.coldDcrOhmsAt20C *
            (1.0 + rules.copperTempCoefficientPerC * (rules.assumedWindingTempCWhenThermalNotEvaluated - 20.0));

        result.resistanceStatus = EvaluationStatus::Evaluated;
    } 
    else {
        result.resistanceStatus = EvaluationStatus::NotEvaluated;
        result.missingData.push_back(
            "core '" + core.partNumber +
            "' has no mean-length-per-turn estimate available - cannot compute total wire length or DCR");
    }

    return result;
}
