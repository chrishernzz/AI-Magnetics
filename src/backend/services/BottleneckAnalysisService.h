#pragma once
#include "core/model/BottleneckAnalysis.h"
#include "core/model/InductorCandidate.h"

//precondition: candidate has been through evaluateCandidate() - either the early turns/gap
//non-convergence return, or the full validation path
//postcondition: identifies the single check most responsible for this candidate's current standing -
//see BottleneckAnalysis.h for the three distinct reasons and why they're kept separate. Never
//fabricates a margin for a check that didn't run.
BottleneckAnalysis analyzeBottleneck(const InductorCandidate& candidate);
