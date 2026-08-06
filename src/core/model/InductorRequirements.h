#pragma once
#include <optional>
#include <string>
#include "validation/EvaluationStatus.h"
#include "core/model/ConductionMode.h"

/*

STAGE 0: Normalized internal requirements
This is the C++-side counterpart of the API's InductorDesignRequest,after RequirementDerivationService has converted units (uH -> H, kHz ->
Hz) and derived any values that can be derived (e.g. rmsCurrentA from average current + ripple, for a triangular ripple waveform). Every stage
downstream of RequirementDerivationService reads from this struct, never
from the raw request, so there is exactly one place unit conversion and current-field derivation happens.

*/
struct OperatingPoint {
    double inductanceH;
    //Optional - see InductorDesignRequest::peakCurrentA for why this can never be substituted from RMS.
    std::optional<double> peakCurrentA;
    double rmsCurrentA;
    double switchingFreqHz;

    //true if peakCurrentA was derived from rmsCurrentA + rippleCurrentPeakToPeakA (triangular-ripple identity: Iavg = sqrt(Irms^2 - ripple^2/12), peak = Iavg + ripple/2 - the algebraic inverse of the
    //same triangular-ripple formula rmsCurrentAssumption below already uses) rather than supplied directly. Callers must surface peakCurrentAssumption whenever this is true - a derived peak is real math, 
    bool peakCurrentDerived = false;
    std::string peakCurrentAssumption;

    //true if rmsCurrentA was derived from averageCurrentA + ripple rather than supplied directly - callers must surface rmsCurrentAssumption
    //whenever this is true (spec section 5: "the system must state that this relationship assumes triangular ripple").
    bool rmsCurrentDerived = false;
    std::string rmsCurrentAssumption;

    //Present only if the request supplied it directly - carried through regardless of which branch rmsCurrentA came from, since core loss (LossEvaluation) needs the real ripple swing, not the RMS value, to
    //compute flux-density swing. Never inferred from anything else.
    std::optional<double> rippleCurrentPeakToPeakA;

    //Current-consistency check (spec: peak/RMS/ripple must describe one physically real waveform, never three independent numbers). Evaluated only when BOTH peakCurrentA and rippleCurrentPeakToPeakA are
    //supplied - those are the only two values a minimum instantaneous inductor current can be derived from. See RequirementDerivationService::derive() for the computation. A genuine physical
    //contradiction (implied DCM, or an out-of-envelope rmsCurrentA) never throws and never blocks design generation - it sets this status to NotEvaluated with currentConsistencyExplanation stating
    //why, exactly like any other missing-data case. The candidate is still evaluated on everything that doesn't depend on this (saturation via peak alone, fill/current-density via RMS, core loss from the
    //literal ripple value regardless of whether it's consistent with peak/RMS).
    EvaluationStatus currentConsistencyStatus = EvaluationStatus::NotEvaluated;
    //valid only when currentConsistencyStatus == Evaluated. Triangular-ripple assumption: minInductorCurrentA
    //= peakCurrentA - rippleCurrentPeakToPeakA. Still populated (when computable) even when status is
    //NotEvaluated due to a contradiction, so the explanation can cite the real numbers.
    ConductionMode conductionMode = ConductionMode::CCM;
    double minInductorCurrentA = 0.0;
    std::string currentConsistencyExplanation;
};

struct InductorRequirements {
    OperatingPoint operatingPoint;

    double ambientTemperatureC;
    double allowableTempRiseC;
    double inductanceTolerancePercent;

    //Optional inputs (spec section 5) - present only if the request supplied them. Nothing downstream may invent a value for these.
    std::optional<double> maximumDcrMilliOhm;
    std::optional<double> maximumWidthMm;
    std::optional<double> maximumHeightMm;
    std::optional<double> maximumLengthMm;
    std::optional<std::string> preferredMaterialFamily;
    std::optional<std::string> preferredCoreGeometry;
};
