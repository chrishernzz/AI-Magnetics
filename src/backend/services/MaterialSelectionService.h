#pragma once
#include "../../core/MaterialSelection.h"

class MaterialSelectionService {
public:
    MaterialSelectionResult calculate(const MaterialSelectionInput& input);
};