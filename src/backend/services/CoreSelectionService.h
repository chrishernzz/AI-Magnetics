#pragma once
#include "../../core/CoreSelection.h"

class CoreSelectionService {
public:
    CoreSelectionResult calculate(const CoreSelectionInput& input);
};