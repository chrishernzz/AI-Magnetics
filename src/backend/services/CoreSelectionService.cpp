#include "CoreSelectionService.h"

//precondition: none
//postcondition: returns the selectCore from CoreSelection.cpp (this calls the business logic)
CoreSelectionResult CoreSelectionService::calculate(const CoreSelectionInput& input) {
    return selectCore(input);
}