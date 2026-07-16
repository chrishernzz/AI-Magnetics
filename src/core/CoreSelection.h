#pragma once
#include <string>

// Struct-only header now: the legacy single-pick selectCore() implementation
// (CoreSelection.cpp) was removed once nothing called it anymore - the
// Phase 1 pipeline uses CoreEvaluation.cpp's findSuitableCores() instead.
// This header stays because TurnsCalculation.h still embeds
// CoreSelectionResult (reused as the seed-turns input inside
// TurnsAndGapDesign.cpp) - deleting it would break that real dependency.

struct CoreSelectionInput {
    double areaProduct;
    double peakCurrentA;
    std::string recommendedMaterial;
};

struct CoreSelectionResult {
    std::string partNumber;
    std::string material;
    double mu;
    double al;
    double ae;
    double wa;
    double le;
};
