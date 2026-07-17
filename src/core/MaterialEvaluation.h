#pragma once
#include <string>
#include <vector>
#include "InductorRequirements.h"

/*STAGE 1: Material candidate evaluation

This header/cpp file will be able to get all the materials from the database
'real_materials.csv' and evaluate them against the requested switching frequency and 
return a list of all the materials that are suitable for the requested switching frequency.
Each material will be returned as a MaterialCandidate struct, which will contain the material's name, muOpt, 
and whether it has Bmax and core loss data. It will also contain any missing data warnings for each material. 

*/

//precondition: none
//postcondition: data will be used to carry the material candidate list through the pipeline
struct MaterialCandidate {
    std::string materialFamily;
    double muOpt;

    //true if the material's declared [minFrequencyHz, maxFrequencyHz) range contains the requested switching frequency.
    bool frequencySuitable;

    //whether real_materials.csv carries non-placeholder data for this material's saturation flux density / core-loss coefficients. Both are 0.0 (unpopulated) for every material in the current snapshot -
    //this flag exists so downstream stages (and the API response) can say so honestly instead of silently treating 0.0 as a real value.
    bool hasBmaxData;
    bool hasCoreLossData;

    //raw values from real_materials.csv - only meaningful when the corresponding hasXData flag above is true. 0.0 by default, matching "no data" rather than a real material property.
    double bmaxT = 0.0;
    double cuLossFactor = 0.0;

    std::string reason;
    std::string alternatives;
    std::vector<std::string> missingDataWarnings;
};

//precondition: Materials::load() has been populated (set_material_database called at FastAPI startup)
//postcondition: returns every material whose declared frequency range, contains operatingPoint.switchingFreqHz, each as a full candidate with its own reason and missing-data warnings - not a single winner.
std::vector<MaterialCandidate> findSuitableMaterials(const OperatingPoint& operatingPoint);
