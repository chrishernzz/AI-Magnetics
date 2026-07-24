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
}  // namespace

//precondition: input.topology == Topology::Buck; every numeric field is finite; vinMinV, vinMaxV, ioutA, switchingFreqKHz, rippleCurrentPercent are all positive; vinMinV <= vinMaxV; vinMinV > voutV > 0 (a buck converter cannot regulate Vout >= Vin_min - see header)
//postcondition: returns an InductorDesignRequest with inductanceUH, peakCurrentA, switchingFreqKHz, averageCurrentA, and rippleCurrentPeakToPeakA populated - rmsCurrentA is left unset so
//RequirementDerivationService derives it downstream using the exact same triangular-ripple formula Mode 2 already relies on
InductorDesignRequest BuckElectricalSolver::solve(const TopologyInput& input) {
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

    //worst-case point for buck ripple current is Vin_max (ripple grows as Vin - Vout grows) - see header comment
    double vin = input.vinMaxV;
    double vout = input.voutV;
    double fswHz = units::kHzToHz(input.switchingFreqKHz);

    //ideal duty cycle, no diode/switch drop modeling in V1 : Formula: D = Vout / Vin
    double dutyCycle = vout / vin;
    //Formula: Iout * (ripple current precent / 100)
    double rippleTargetA = input.ioutA * (input.rippleCurrentPercent / 100.0);
    //Formula: L = (Vin - Vout) * D / (fsw * ripple), the standard buck inductor sizing equation, solved for L given a target ripple current
    double inductanceH = (vin - vout) * dutyCycle / (fswHz * rippleTargetA);
    //Formula: Ipeak = Iout + ripple/2, the standard buck peak current sizing equation, solved for Ipeak given a target ripple current
    double peakCurrent = input.ioutA + rippleTargetA / 2.0;

    //setting the inductor design request to what the BuckElectricalSolver derived from the converter-level requirements, so that the rest of the pipeline can run unchanged regardless of which mode produced the request
    InductorDesignRequest out;
    out.inductanceUH = units::hToUH(inductanceH);
    out.peakCurrentA = peakCurrent;
    out.switchingFreqKHz = input.switchingFreqKHz;
    out.ambientTemperatureC = input.ambientTemperatureC;
    out.allowableTempRiseC = input.allowableTempRiseC;
    out.inductanceTolerancePercent = input.inductanceTolerancePercent;

    //rmsCurrentA intentionally left unset - RequirementDerivationService derives it from these two using the same triangular-ripple formula
    //Mode 2 already uses, so there is exactly one RMS derivation in the whole codebase regardless of which mode produced the request.
    out.averageCurrentA = input.ioutA;
    out.rippleCurrentPeakToPeakA = rippleTargetA;

    return out;
}
