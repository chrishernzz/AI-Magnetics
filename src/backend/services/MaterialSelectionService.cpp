#include "MaterialSelectionService.h"

//precondition: none
//postcondition: returns the selectMaterial from MaterialSelection.cpp (this calls the business logic)
MaterialSelectionResult MaterialSelectionService::calculate(const MaterialSelectionInput& input) {
    return selectMaterial(input);
}