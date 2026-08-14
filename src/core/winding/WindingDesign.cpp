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

}  //namespace

//precondition: core.waMm2 > 0, turns > 0, rmsCurrentA > 0
//postcondition: selects a round-wire gauge (or parallel strands of rules.minimumSingleStrandAwg if a single strand would be impractically thick) meeting rules.allowableCurrentDensityAperCm2, and computes fill
//factor / current density against the core's actual window area.
WindingDesignResult designWinding(const CoreCandidate& core, int turns, double rmsCurrentA, const DesignRules& rules) {
    WindingDesignResult result;

    //A/cm^2 -> mm^2 required area: requiredAreaMm2 = I / J_Acm2, converted cm^2 -> mm^2
    double rawRequiredAreaMm2 = units::cm2ToMm2(rmsCurrentA / rules.allowableCurrentDensityAperCm2);
    /*
    rules.wireAreaSafetyMarginPercent adds real headroom on top of the raw current-density minimum - without it, wire selection can pick a gauge that clears the density limit by only a percent or two
    (no margin for DCR temperature rise or manufacturing tolerance). See DesignRules.h for why this number is a documented engineering judgment call, not a sourced fact.
    */
    double requiredAreaMm2 = rawRequiredAreaMm2 * (1.0 + rules.wireAreaSafetyMarginPercent / 100.0);

    const AwgEntry* singleStrand = finestAwgMeetingArea(requiredAreaMm2);
    int selectedAwg = 0;

    if (singleStrand == nullptr || singleStrand->awg < rules.minimumSingleStrandAwg) {
        /*
        Either no single strand in the table is large enough, or the implied gauge is thicker than practical for hand-winding -
        switch to parallel strands of the minimum practical single-strand gauge instead of one very thick solid wire.
        */
        const AwgEntry* strandGauge = findEntry(rules.minimumSingleStrandAwg);
        if (strandGauge == nullptr) {
            //fallback to the thickest table entry
            strandGauge = &kAwgTable[0]; 
        }

        int strands = std::max(1, static_cast<int>(std::ceil(requiredAreaMm2 / strandGauge->areaMm2)));

        result.wireDescription = std::to_string(strands) + "x AWG" + std::to_string(strandGauge->awg) + ", single-build magnet wire";
        result.conductorAreaMm2 = strandGauge->areaMm2;
        result.parallelStrands = strands;
        selectedAwg = strandGauge->awg;
    }
    else {
        result.wireDescription = "1x AWG" + std::to_string(singleStrand->awg) + ", single-build magnet wire";
        result.conductorAreaMm2 = singleStrand->areaMm2;
        result.parallelStrands = 1;
        selectedAwg = singleStrand->awg;
    }

    double totalCopperAreaPerTurnMm2 = result.conductorAreaMm2 * result.parallelStrands;

    result.currentDensityAperMm2 = rmsCurrentA / totalCopperAreaPerTurnMm2;
    result.effectiveCurrentDensityAperMm2 = result.parallelStrands > 1 ? result.currentDensityAperMm2 / rules.currentSharingDerateFactor : result.currentDensityAperMm2;
    /*
    core.waMm2 <= 0 means "no real window-area data for this core" (see CoreCandidate::waMm2 / data/real_cores.csv's known E-core gap: mag-inc.com's part search exposes external Length/Leg/Width,
    not bobbin window dimensions), not a literal zero-area window. Dividing by it would silently produce an infinite fillFactor and an unconditional fitsWindow=false, which reads as "definitely does not
    fit" - a fabricated conclusion this project's spec explicitly forbids (section 11). Fail safe instead: report not-fitting with an honest missing-data explanation, the same "don't invent, don't assume
    passing" policy resistanceStatus already follows below for missing MLT.
    */
    if (core.waMm2 > 0.0) {
        result.fillFactor = (static_cast<double>(turns) * totalCopperAreaPerTurnMm2) / core.waMm2;
        result.fitsWindow = result.fillFactor <= rules.maximumFillFactor;
    } 
    else {
        result.fillFactor = 0.0;
        result.fitsWindow = false;
        result.missingData.push_back("core '" + core.partNumber + "' has no real winding-window area (Wa) data available - fillFactor/fitsWindow cannot be computed and are conservatively reported as not fitting rather than assumed");
    }

    result.constructionType = result.parallelStrands > 1 ? WindingConstructionType::ParallelRoundWires : WindingConstructionType::SingleRoundWire;

    //Bare-strand diameter for whichever gauge was actually selected (single or parallel-strand case).
    double bareStrandDiameterMm = 2.0 * std::sqrt(result.conductorAreaMm2 / kPi);
    result.insulatedConductorDiameterMm = bareStrandDiameterMm + rules.singleBuildInsulationBuildUpMm;
    result.insulatedConductorAreaMm2 = (kPi / 4.0) * result.insulatedConductorDiameterMm * result.insulatedConductorDiameterMm;

    if (result.parallelStrands > 1) {
        result.physicalDescription = std::to_string(result.parallelStrands) + "x AWG" + std::to_string(selectedAwg) + " single-build magnet wire (bare " + std::to_string(bareStrandDiameterMm) + " mm, single-build insulated ~" + std::to_string(result.insulatedConductorDiameterMm) + " mm each), wound as a single bundle";

        /*
        Real check, only possible for two-piece cores (real window width/height - see CoreCandidate::windowWidthMm/windowHeightMm). Toroids have no flat width/height at all (radial
        window geometry instead), so they - and any two-piece core still missing this data - stay permanently NotEvaluated here, never assumed to fit.
        */
        if (core.windowWidthMm > 0.0 && core.windowHeightMm > 0.0) {
            result.bundleWidthMm = static_cast<double>(result.parallelStrands) * result.insulatedConductorDiameterMm;
            result.narrowestWindowOpeningMm = std::min(core.windowWidthMm, core.windowHeightMm);
            result.bundleFitsWindowOpening = result.bundleWidthMm <= result.narrowestWindowOpeningMm;
            result.bundleFitStatus = EvaluationStatus::Evaluated;
        } 
        else {
            result.missingData.push_back(
                "parallel-strand bundle-vs-narrowest-opening fit is not evaluated - this core has no real "
                "window width/height (toroid, or a two-piece core missing that data in the current snapshot)");
        }
    } 
    else {
        result.physicalDescription = "1x AWG" + std::to_string(selectedAwg) + " single-build magnet wire (bare " +
            std::to_string(bareStrandDiameterMm) + " mm, single-build insulated ~" + std::to_string(result.insulatedConductorDiameterMm) + " mm)";
    }

    /*
    Physical window fill: bobbin-wall derate (non-toroid cores only - see below), then subtract margin/lead-exit clearance (both expressed as area fractions in DesignRules.h - real window
    width/height now exists for two-piece cores, see CoreCandidate::windowWidthMm/windowHeightMm, but this formula does not yet switch to a literal-mm margin for them; toroids still have no linear window
    dimension at all), then divide the insulated-conductor area sum by the achievable packing factor to get the physically occupied area.
    */

    /*
    rules.bobbinWindowDerateFactor models the wall thickness of a physical bobbin former - a real component that TwoPieceSet cores are wound on, but a Toroid has none: it's hand-wound directly around
    the core, with no separate former consuming window space. Applying that derate uniformly to every core shape overstated how much window a toroid loses - a real user report (a senior magnetics
    engineer's hand calculation) caught this, and it was the single largest identifiable cause of powder toroids (which already need far more turns than ferrite for the same inductance, since powder's real
    permeability is 10-100x lower) failing this check. Margin/lead-exit clearance still applies to toroids - hand winding still needs real clearance space - only the bobbin-specific derate is skipped.
    */
    bool coreHasPhysicalBobbin = core.coreShape != "Toroid";
    double bobbinDerate = coreHasPhysicalBobbin ? rules.bobbinWindowDerateFactor : 1.0;
    result.physicalWindowAreaMm2 = core.waMm2 * bobbinDerate * (1.0 - rules.marginAllowanceAreaFraction - rules.leadExitAllowanceAreaFraction);
    double physicalCopperAreaMm2 = (static_cast<double>(turns) * result.parallelStrands * result.insulatedConductorAreaMm2) / rules.packingFactor;
    if (core.waMm2 > 0.0) {
        result.physicalWindowFillFactor = physicalCopperAreaMm2 / result.physicalWindowAreaMm2;
        result.fitsPhysicalWindow = result.physicalWindowFillFactor <= rules.maximumFillFactor;
    } 
    else {
        /*
        core.waMm2 <= 0 ("no real window-area data", see fillFactor/fitsWindow above) previously fell through the ternary below to physicalWindowAreaMm2==0.0 -> physicalWindowFillFactor=0.0 ->
        fitsPhysicalWindow=true - a false PASS on the actual gate WindingFitValidation uses, the opposite and more dangerous failure mode than fitsWindow's false-negative above (this one would recommend
        an unverified core to a user). Same conservative "not fitting" policy as fillFactor/fitsWindow.
        */
        result.physicalWindowFillFactor = 0.0;
        result.fitsPhysicalWindow = false;
    }

    if (core.mltMm > 0.0) {
        //Length of one strand's path all the way around the core, turns times over - not yet divided by parallel strands.
        result.coreWindingLengthM = units::mmToM(static_cast<double>(turns) * core.mltMm);
        result.leadLengthM = units::mmToM(rules.totalLeadLengthAllowanceMm);
        result.routingLengthM = units::mmToM(rules.routingLengthAllowanceMm);
        result.totalLengthM = result.coreWindingLengthM + result.leadLengthM + result.routingLengthM;

        double conductorAreaM2 = units::mm2ToM2(result.conductorAreaMm2);
        double singleStrandResistanceOhms = kCopperResistivityOhmMAt20C * result.totalLengthM / conductorAreaM2;
        result.connectionResistanceOhms = units::milliOhmToOhm(rules.connectionResistanceMilliOhm);

        result.totalWireLengthM = result.coreWindingLengthM * result.parallelStrands;
        //Paralleling N identical strands divides resistance by N; the termination joint is a single connection, not itself paralleled across strands.
        result.coldDcrOhmsAt20C = singleStrandResistanceOhms / result.parallelStrands + result.connectionResistanceOhms;
        result.dcrOhms = result.coldDcrOhmsAt20C;

        //Conservative sanity-check estimate only - not a converged answer. Overwritten by the real iterative thermal loop's converged hot DCR once that loop exists (see ThermalEvaluation.h).
        result.estimatedHotDcrOhms = result.coldDcrOhmsAt20C *
            (1.0 + rules.copperTempCoefficientPerC * (rules.assumedWindingTempCWhenThermalNotEvaluated - 20.0));

        result.resistanceStatus = EvaluationStatus::Evaluated;
    } 
    else {
        result.resistanceStatus = EvaluationStatus::NotEvaluated;
        result.missingData.push_back("core '" + core.partNumber + "' has no mean-length-per-turn estimate available - cannot compute total wire length or DCR");
    }

    return result;
}
