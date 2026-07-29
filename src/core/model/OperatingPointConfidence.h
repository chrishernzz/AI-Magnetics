#pragma once
#include <string>

/*

STAGE 13: Operating-point confidence classification

Names which of the request's optional current inputs (peakCurrentA, rippleCurrentPeakToPeakA) were
actually supplied, and where the value the engine is using for peak current actually came from - a
directly-supplied number is not the same trust level as one this engine derived (see
RequirementDerivationService::derive()'s Mode 2 peak-current derivation), even though both let
PeakFluxValidation/SaturationValidation genuinely evaluate. Computed once per request from the inputs
alone - never per-candidate, since it doesn't depend on which core/material is being evaluated - and
attached to DesignRecommendation, the same "one blob describing this whole run" pattern already used
for activeRules/versions.

This is a separate, orthogonal signal from RecommendationTier (RecommendationStatus.h): tier already
captures per-check data completeness/quality (isPreliminaryEstimate, usesDefaultAssumption,
EvaluationStatus) for each of a candidate's mandatory checks. OperatingPointConfidence instead answers
a request-level question - "how much did the user tell us" - independent of which candidate you're
looking at.

*/

//RmsOnly: only inductance/rmsCurrentA/switchingFreqHz supplied - saturation/peak-flux cannot be
//evaluated at all (no Ipk to compute Bpk from).
//RmsPlusRipple: ripple also supplied but peak was not - saturation MAY still be evaluated if peak was
//successfully derived (see PeakCurrentProvenance::Derived below); if the derivation's own
//consistency check failed (Irms^2 < ripple^2/12), it's effectively still RmsOnly for saturation
//purposes even though this mode is reported.
//RmsPlusPeak: peakCurrentA was supplied directly (regardless of whether ripple was also supplied).
//FullOperatingPoint: peakCurrentA AND rippleCurrentPeakToPeakA were both supplied directly.
enum class OperatingPointInputMode {
    RmsOnly,
    RmsPlusRipple,
    RmsPlusPeak,
    FullOperatingPoint,
};

//NotSupplied: peakCurrentA is absent - PeakFluxValidation/SaturationValidation report NotEvaluated.
//Derived: peakCurrentA was computed from rmsCurrentA + rippleCurrentPeakToPeakA (a real algebraic
//derivation, not a default) - see OperatingPoint::peakCurrentDerived. Checks that consume it run for
//real, but the value is not a directly measured/supplied number and must always be labeled as such.
//DirectlySupplied: the request supplied peakCurrentA itself - the highest-trust case.
enum class PeakCurrentProvenance {
    NotSupplied,
    Derived,
    DirectlySupplied,
};

struct OperatingPointConfidence {
    OperatingPointInputMode inputMode = OperatingPointInputMode::RmsOnly;
    PeakCurrentProvenance peakCurrentProvenance = PeakCurrentProvenance::NotSupplied;
    //human-readable summary combining the two fields above into one sentence, e.g. "RMS + ripple
    //supplied; peak current derived from RMS and ripple (triangular-ripple assumption), not directly
    //measured - saturation validation runs against the derived value."
    std::string summary;
};
