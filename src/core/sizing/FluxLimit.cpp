#include "core/sizing/FluxLimit.h"

//precondition: none
//postcondition: see header
FluxLimit applicableFluxLimit(const MaterialCandidate& material, const DesignRules& rules) {
    if (material.hasBmaxData) {
        return {material.bmaxT, false};
    }
    return {rules.defaultFluxDensityLimitT, true};
}
