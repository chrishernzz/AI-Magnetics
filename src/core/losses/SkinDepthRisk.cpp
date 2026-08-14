#include "core/losses/SkinDepthRisk.h"
#include "core/units/UnitConversions.h"
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
/*
Annealed copper resistivity at 20C, ohm-meters - same real IACS value WindingDesign.cpp uses for DCR. Duplicated locally rather than shared, matching this codebase's existing pattern of each calculation
module owning its own physical constants (see GapDesign.cpp's local kPi).
*/
constexpr double kCopperResistivityOhmMAt20C = 1.724e-8;
//Vacuum permeability, H/m - a real physical constant.
constexpr double kMu0HPerM = 4.0 * kPi * 1e-7;
}  //namespace

//precondition: switchingFreqHz > 0, conductorAreaMm2 > 0 (one strand's bare cross-section area)
//postcondition: see struct field comments above. riskLevel thresholds come from rules.skinDepthRiskModerateThreshold/skinDepthRiskHighThreshold - a Phase 1 heuristic boundary, not a validated Dowell/FEA AC-loss limit.
SkinDepthRiskResult evaluateSkinDepthRisk(double switchingFreqHz, double conductorAreaMm2, const DesignRules& rules) {
    SkinDepthRiskResult result;

    //delta = sqrt(rho / (pi * f * mu0)) - classical skin depth, SI (meters), then converted to mm.
    double skinDepthM = std::sqrt(kCopperResistivityOhmMAt20C / (kPi * switchingFreqHz * kMu0HPerM));
    result.skinDepthMm = units::mToMm(skinDepthM);

    double conductorAreaM2 = units::mm2ToM2(conductorAreaMm2);
    double strandRadiusM = std::sqrt(conductorAreaM2 / kPi);
    result.strandRadiusMm = units::mToMm(strandRadiusM);

    result.radiusToSkinDepthRatio = result.skinDepthMm > 0.0 ? result.strandRadiusMm / result.skinDepthMm : 0.0;

    if (result.radiusToSkinDepthRatio <= rules.skinDepthRiskModerateThreshold) {
        result.riskLevel = AcLossRiskLevel::Low;
    } 
    else if (result.radiusToSkinDepthRatio <= rules.skinDepthRiskHighThreshold) {
        result.riskLevel = AcLossRiskLevel::Moderate;
    }
    else {
        result.riskLevel = AcLossRiskLevel::High;
    }

    result.reason = "evaluated: single-strand skin effect (bare strand radius " + std::to_string(result.strandRadiusMm) + " mm vs skin depth " + std::to_string(result.skinDepthMm) + " mm at " + std::to_string(switchingFreqHz) + " Hz, ratio " + std::to_string(result.radiusToSkinDepthRatio) + "). NOT evaluated: proximity effect between bundled parallel strands, and proximity to the air gap - no winding-layer or winding-to-gap geometry exists in this engine to compute either from.";

    return result;
}
