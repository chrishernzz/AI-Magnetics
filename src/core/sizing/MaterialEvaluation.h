#pragma once
#include <string>
#include <vector>
#include "core/model/InductorRequirements.h"
#include "core/model/Provenance.h"

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
    double muOpt = 0.0;

    //true if the material's declared [minFrequencyHz, maxFrequencyHz) range contains the requested switching frequency.
    bool frequencySuitable = false;

    /*
    hasBmaxData: true when real_materials.csv carries a real (>0.0) saturation flux density for this
    material - true for most materials in the current snapshot (e.g. 3C90=0.47T), false for the handful
    that don't (see scripts/audit_material_core_database.py for the current count).
    hasCoreLossData: true when data/real_core_loss_coefficients.csv carries at least one real Steinmetz
    coefficient row for this material, at any frequency (checked against the actual coefficient database,
    not the unused cuLossFactor field below - see MaterialEvaluation.cpp's hasAnyCoreLossCoefficients()).
    Both flags exist so downstream stages (and the API response) say so honestly instead of silently
    treating missing data as a real value. Defaulted to false (not left indeterminate) - a struct with no
    in-class initializer here is genuinely uninitialized garbage when default-constructed, a real latent
    bug found via a g++-11-specific test failure: a default-constructed MaterialCandidate in a test (never
    explicitly setting hasBmaxData) read as true under one compiler/build and false under another, silently
    flipping whether LossEvaluation.cpp's saturation check ran at all.
    */
    bool hasBmaxData = false;
    bool hasCoreLossData = false;

    /*
    bmaxT: real value from real_materials.csv when hasBmaxData is true, 0.0 otherwise.
    cuLossFactor: always 0.0 - real_materials.csv carries no such column in the current snapshot; core-loss
    coefficients live entirely in the separate real_core_loss_coefficients.csv (see hasCoreLossData above).
    Kept as a field rather than removed since MaterialData (the raw CSV row type) still declares it.
    */
    double bmaxT = 0.0;
    double cuLossFactor = 0.0;

    std::string reason;
    std::string alternatives;
    std::vector<std::string> missingDataWarnings;

    /*
    provenance for this material's data - see Provenance.h. datasheetRevision/Url/dateAccessed are always
    unset in Phase 1 (no real per-material datasheet metadata exists in this snapshot).
    */
    SourceInfo source;
};

std::vector<MaterialCandidate> findSuitableMaterials(const OperatingPoint& operatingPoint);
