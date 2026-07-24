#include "BuckElectricalSolver.h"
#include <stdexcept>
#include "core/units/UnitConversions.h"

//precondition: input.topology == Topology::Buck; vinMinV, vinMaxV, ioutA, switchingFreqKHz, rippleCurrentPercent are all positive; vinMinV <= vinMaxV; vinMaxV > voutV > 0 (a buck converter cannot regulate Vout >= Vin)
//postcondition: returns an InductorDesignRequest with inductanceUH, peakCurrentA, switchingFreqKHz, averageCurrentA, and rippleCurrentPeakToPeakA populated - rmsCurrentA is left unset so
//RequirementDerivationService derives it downstream using the exact same triangular-ripple formula Mode 2 already relies on
InductorDesignRequest BuckElectricalSolver::solve(const TopologyInput& input) {
    //base case checking if its the right topology, making sure no negative inputs, Vin min not greater than max, and Vout to not be negative and less than Vin Max
    if (input.topology != Topology::Buck) {
        throw std::invalid_argument("BuckElectricalSolver::solve called with a non-Buck TopologyInput");
    }
    if (input.vinMinV <= 0.0 || input.vinMaxV <= 0.0 || input.ioutA <= 0.0 || input.switchingFreqKHz <= 0.0 || input.rippleCurrentPercent <= 0.0) {
        throw std::invalid_argument("vinMinV, vinMaxV, ioutA, switchingFreqKHz, and rippleCurrentPercent must all be positive");
    }
    if (input.vinMinV > input.vinMaxV) {
        throw std::invalid_argument("vinMinV cannot exceed vinMaxV");
    }
    if (input.voutV <= 0.0 || input.voutV >= input.vinMaxV) {
        throw std::invalid_argument("voutV must be positive and less than vinMaxV - a buck converter cannot regulate Vout >= Vin");
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
