#pragma once
#include<string>
/*
STAGE 1: Select Material / Calculate μopt

Input:  target ripple / frequency operating point
Output: recommended core material family (e.g. ferrite grade,
        powder core mix) and the optimal permeability (μopt)
        for minimizing total loss at the given frequency.

Reference: McLyman, "Transformer and Inductor Design Handbook",
p.63 flowchart — this is the first block: Specifications -> Select Material.
*/

//precondition: none
//postcondition: this data inside will be used to pick the material selection (user inputs this)
struct MaterialSelectionInput {
    double inductanceH;
    double peakCurrentA;
    double switchingFreqHz;
    double allowableTempRiseC;
    double waveformFactor;
};

//precondition: none
//postcondition: this data will be returned back to user
struct MaterialSelectionResult {
    std::string materialFamily;
    double muOpt;
    std::string reason;
    std::string alternatives;
};

MaterialSelectionResult selectMaterial(const MaterialSelectionInput& in);
