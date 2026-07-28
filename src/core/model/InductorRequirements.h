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
    double peakCurrentA;
    double rmsCurrentA;
    double switchingFreqHz;

    //true if rmsCurrentA was derived from averageCurrentA + ripple rather
    //than supplied directly - callers must surface rmsCurrentAssumption
    //whenever this is true (spec section 5: "the system must state that
    //this relationship assumes triangular ripple").
    bool rmsCurrentDerived = false;
    std::string rmsCurrentAssumption;

    //Present only if the request supplied it directly - carried through
    //regardless of which branch rmsCurrentA came from, since core loss
    //(LossEvaluation) needs the real ripple swing, not the RMS value, to
    //compute flux-density swing. Never inferred from anything else.
    std::optional<double> rippleCurrentPeakToPeakA;

    //Current-consistency check (spec: peak/RMS/ripple must describe one physically real waveform, never
    //three independent numbers). Evaluated only when rippleCurrentPeakToPeakA is supplied - that is the
    //only case where a minimum instantaneous inductor current can be derived at all. See
    //RequirementDerivationService::derive() for the computation; a physically impossible combination
    //(negative minimum current, or an out-of-envelope rmsCurrentA) throws std::invalid_argument there
    //rather than silently proceeding, so this struct only ever carries a self-consistent result.
    EvaluationStatus currentConsistencyStatus = EvaluationStatus::NotEvaluated;
    //valid only when currentConsistencyStatus == Evaluated. Triangular-ripple assumption: minInductorCurrentA
    //= peakCurrentA - rippleCurrentPeakToPeakA.
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
