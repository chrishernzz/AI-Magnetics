#pragma once
#include "core/model/InductorRequirements.h"
#include "core/model/OperatingPointConfidence.h"

//precondition: operatingPoint has already been fully populated by RequirementDerivationService::derive()
//postcondition: classifies which optional inputs the ORIGINAL request supplied (inputMode is never
//fooled by a successful peak derivation into reporting a directly-supplied peak - see
//OperatingPointConfidence.h) and where the peak current value now in use actually came from.
OperatingPointConfidence classifyOperatingPointConfidence(const OperatingPoint& operatingPoint);
