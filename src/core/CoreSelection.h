#pragma once
#include<iostream>
#include <string>

// STAGE 3: Select Core

/*
KEPT from the flyback version, but the logic changes: it now
selects a core by matching Ap against a core database, then
pulls the core's physical parameters from data/cores rather
than from a turns-ratio-driven sizing pass.

Output fields required by the workflow diagram: Ac, Wa, MLT, lc.

*/

//this is what the Core Selection has
struct CoreSelectionInput{
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

CoreSelectionResult selectCore(const CoreSelectionInput& input);

