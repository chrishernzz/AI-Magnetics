#pragma once
#include <string>

/*

STAGE 14: Primary bottleneck / limiting-factor identification

Names the ONE check that is most responsible for a candidate's current standing - the failed check
closest to passing (a rejected candidate), the mandatory check that couldn't run at all (a
data-completeness gap, e.g. no peak current supplied), or the evaluated check with the least
headroom (a passing candidate - "which constraint would this design hit first if conditions got
slightly worse"). These are three genuinely different engineering questions, kept as distinct
reasons rather than collapsed into one number, so a caller never mistakes "we don't know" for "it's
nearly failing."

*/

enum class BottleneckReason {
    //A mandatory check ran and failed - this is why the candidate was rejected. Only set on rejected
    //candidates.
    FailedCheck,
    //A mandatory check never ran because required data wasn't supplied (e.g. PeakFluxValidation/
    //SaturationValidation with no peakCurrentA) - a data-completeness gap, not a physical limitation.
    //Always takes priority over MarginHeadroom on a passing/conditional-pass candidate, since "we don't
    //know" is a more important caveat than "it has plenty of margin on the checks we could run."
    NotEvaluatedCheck,
    //Every mandatory check ran and passed - this is the evaluated check with the smallest normalized
    //margin (margin / limitValue) above its limit, i.e. the one that would fail first if the design's
    //real-world conditions got slightly worse. Only set on candidates with no FailedCheck/
    //NotEvaluatedCheck to report.
    MarginHeadroom,
    //No validations to analyze (e.g. turns/gap never converged - see BottleneckAnalysisService.cpp).
    None,
};

struct BottleneckAnalysis {
    BottleneckReason reason = BottleneckReason::None;
    //the check name this analysis is about (e.g. "SaturationValidation"), or the literal string
    //"TurnsAndGapDesign" for the pre-validation non-convergence case - empty when reason == None.
    std::string limitingCheckName;
    std::string explanation;
    //valid only when marginValueApplicable - the raw (FailedCheck) or normalized (MarginHeadroom)
    //margin value that drove the selection. Never meaningful for NotEvaluatedCheck/None.
    double marginValue = 0.0;
    bool marginValueApplicable = false;
};
