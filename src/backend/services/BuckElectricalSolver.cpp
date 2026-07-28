#include "BuckElectricalSolver.h"
#include <cmath>
#include <stdexcept>
#include <string>
#include "core/units/UnitConversions.h"

namespace {
//precondition: none
//postcondition: throws std::invalid_argument naming the field if value is NaN or +-infinity - never lets a
//non-finite value silently propagate into a formula and produce a non-finite (and misleadingly "successful") result
void requireFinite(double value, const char* fieldName) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(fieldName) + " must be a finite number (got NaN or infinity)");
    }
}

//floating-point comparison tolerance for the CCM/DCM boundary check - not a design assumption, just enough
//slack that a minimum current that is mathematically ~0 doesn't misclassify as negative from rounding.
constexpr double kCcmZeroEpsilonA = 1e-6;

//precondition: fswHz > 0, inductanceH > 0
//postcondition: returns the real electrical quantities at this specific Vin, using the inductance that was
//actually sized (at Vin_max) - not an independent re-sizing at this Vin.
BuckOperatingPointResult evaluateAtVin(double vinV, double voutV, double ioutA, double fswHz, double inductanceH) {
    BuckOperatingPointResult r;
    r.vinV = vinV;
    //ideal duty cycle, no diode/switch drop modeling in V1 : Formula: D = Vout / Vin
    r.dutyCycle = voutV / vinV;
    //Formula: ripple = (Vin - Vout) * D / (fsw * L), the standard buck ripple-current equation evaluated at
    //this Vin using the fixed sized inductance - not re-derived from a target ripple percent per point.
    r.rippleCurrentPeakToPeakA = (vinV - voutV) * r.dutyCycle / (fswHz * inductanceH);
    r.peakCurrentA = ioutA + r.rippleCurrentPeakToPeakA / 2.0;
    r.minInductorCurrentA = ioutA - r.rippleCurrentPeakToPeakA / 2.0;
    return r;
}
}  // namespace

//precondition: input.topology == Topology::Buck; every numeric field is finite; vinMinV, vinMaxV, ioutA, switchingFreqKHz, rippleCurrentPercent are all positive; vinMinV <= vinMaxV; vinMinV > voutV > 0 (a buck converter cannot regulate Vout >= Vin_min - see header)
//postcondition: returns a BuckSolveResult with the derived InductorDesignRequest (sized at Vin_max), real electrical quantities at both Vin_min and Vin_max, which operating point is worst per quantity,
//CCM/DCM classification, and the stated Phase 1 assumptions - rmsCurrentA on the request is left unset so RequirementDerivationService derives it downstream using the exact same triangular-ripple formula Mode 2 already relies on
BuckSolveResult BuckElectricalSolver::solve(const TopologyInput& input, const DesignRules& rules) {
    //base case checking if its the right topology, making sure no negative inputs, Vin min not greater than max, and Vout to not be negative and less than Vin Min
    if (input.topology != Topology::Buck) {
        throw std::invalid_argument("BuckElectricalSolver::solve called with a non-Buck TopologyInput");
    }

    requireFinite(input.vinMinV, "vinMinV");
    requireFinite(input.vinMaxV, "vinMaxV");
    requireFinite(input.voutV, "voutV");
    requireFinite(input.ioutA, "ioutA");
    requireFinite(input.switchingFreqKHz, "switchingFreqKHz");
    requireFinite(input.rippleCurrentPercent, "rippleCurrentPercent");
    requireFinite(input.ambientTemperatureC, "ambientTemperatureC");
    requireFinite(input.allowableTempRiseC, "allowableTempRiseC");

    if (input.vinMinV <= 0.0 || input.vinMaxV <= 0.0 || input.ioutA <= 0.0 || input.switchingFreqKHz <= 0.0 || input.rippleCurrentPercent <= 0.0) {
        throw std::invalid_argument("vinMinV, vinMaxV, ioutA, switchingFreqKHz, and rippleCurrentPercent must all be positive");
    }
    if (input.rippleCurrentPercent > rules.maximumRippleCurrentPercent) {
        throw std::invalid_argument("rippleCurrentPercent (" + std::to_string(input.rippleCurrentPercent) +
                                     "%) exceeds the Phase 1 input-sanity bound (" + std::to_string(rules.maximumRippleCurrentPercent) +
                                     "%) - above this the triangular-ripple CCM model is unreliable and the operating point is likely already in DCM");
    }
    if (input.vinMinV > input.vinMaxV) {
        throw std::invalid_argument("vinMinV cannot exceed vinMaxV");
    }
    //the real binding constraint is Vout vs Vin_min, not Vin_max: at Vin = vinMinV the required duty cycle
    //(D = Vout/Vin) is at its maximum, so a converter that could regulate at high line may be unable to at
    //low line even though Vout < vinMaxV would suggest it can. vinMinV <= vinMaxV already holds here, so
    //this check is strictly stronger than (and supersedes) a Vout-vs-vinMaxV check.
    if (input.voutV <= 0.0 || input.voutV >= input.vinMinV) {
        throw std::invalid_argument("voutV must be positive and less than vinMinV - a buck converter cannot regulate Vout >= Vin at the low end of the input range");
    }

    double fswHz = units::kHzToHz(input.switchingFreqKHz);

    //worst-case point for buck ripple current is Vin_max (ripple grows as Vin - Vout grows) - inductance is
    //sized here, then the real electrical quantities are evaluated at both Vin_min and Vin_max below.
    double dutyAtMax = input.voutV / input.vinMaxV;
    //Formula: Iout * (ripple current percent / 100)
    double rippleTargetA = input.ioutA * (input.rippleCurrentPercent / 100.0);
    //Formula: L = (Vin - Vout) * D / (fsw * ripple), the standard buck inductor sizing equation, solved for L given a target ripple current
    double inductanceH = (input.vinMaxV - input.voutV) * dutyAtMax / (fswHz * rippleTargetA);

    BuckSolveResult out;
    out.atVinMax = evaluateAtVin(input.vinMaxV, input.voutV, input.ioutA, fswHz, inductanceH);
    out.atVinMin = evaluateAtVin(input.vinMinV, input.voutV, input.ioutA, fswHz, inductanceH);

    out.worstCaseDutyCycleAt = out.atVinMin.dutyCycle > out.atVinMax.dutyCycle ? "VinMin" : "VinMax";
    out.worstCaseRippleAt = out.atVinMax.rippleCurrentPeakToPeakA >= out.atVinMin.rippleCurrentPeakToPeakA ? "VinMax" : "VinMin";
    out.worstCasePeakAt = out.atVinMax.peakCurrentA >= out.atVinMin.peakCurrentA ? "VinMax" : "VinMin";

    //largest ripple - and therefore the lowest minimum inductor current - happens at Vin_max, so that is the
    //true CCM/DCM boundary check regardless of which point is worst for duty cycle or absolute peak current.
    double worstMinCurrent = out.atVinMax.minInductorCurrentA;
    if (worstMinCurrent < -kCcmZeroEpsilonA) {
        throw std::invalid_argument("computed minimum inductor current is negative at Vin_max (" +
                                     std::to_string(worstMinCurrent) +
                                     " A) - this operating point enters discontinuous conduction mode (DCM), which Phase 1 does not support (CCM only)");
    } else if (worstMinCurrent <= kCcmZeroEpsilonA) {
        out.conductionMode = ConductionMode::CCMBoundary;
        out.warnings.push_back(
            "minimum inductor current is at or near zero at Vin_max - this design sits at the CCM/DCM boundary; "
            "small load or line variation may push it into DCM");
    } else {
        out.conductionMode = ConductionMode::CCM;
    }

    out.assumptions = {
        "ideal converter: no diode or switch forward-voltage drop is modeled",
        "continuous conduction mode (CCM) is assumed - Phase 1 does not model discontinuous conduction mode (DCM)",
        "triangular (piecewise-linear) inductor ripple current waveform is assumed",
        "no PCB trace, connector, or additional parasitic voltage drop is modeled"};

    //setting the inductor design request to what the BuckElectricalSolver derived from the converter-level requirements, so that the rest of the pipeline can run unchanged regardless of which mode produced the request
    out.request.inductanceUH = units::hToUH(inductanceH);
    out.request.peakCurrentA = out.atVinMax.peakCurrentA;
    out.request.switchingFreqKHz = input.switchingFreqKHz;
    out.request.ambientTemperatureC = input.ambientTemperatureC;
    out.request.allowableTempRiseC = input.allowableTempRiseC;
    out.request.inductanceTolerancePercent = input.inductanceTolerancePercent;

    //rmsCurrentA intentionally left unset - RequirementDerivationService derives it from these two using the same triangular-ripple formula
    //Mode 2 already uses, so there is exactly one RMS derivation in the whole codebase regardless of which mode produced the request.
    out.request.averageCurrentA = input.ioutA;
    out.request.rippleCurrentPeakToPeakA = rippleTargetA;

    return out;
}
