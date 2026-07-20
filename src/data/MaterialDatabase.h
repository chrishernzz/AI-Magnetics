#pragma once
#include <string>
#include <vector>
#include "DataCache.h"
//this is our data that material holds from the database 'material.csv'
struct MaterialData {
    std::string name;
    double muOpt;
    double minFrequencyHz;
    double maxFrequencyHz;
    std::string reason;
    std::string alternatives;

    //Material-specific saturation flux density (T) and core-loss coefficient. 0.0 means "no data" (real_materials.csv has these
    //columns but every row is currently 0.0 - see docs/DATA_FILES.md) - callers must treat 0.0 as missing data, never as a real material
    //property, and fall back to DesignRules::defaultFluxDensityLimitT / report core loss as not_evaluated accordingly.
    double bmaxT = 0.0;
    double cuLossFactor = 0.0;
};

class MaterialDatabase {
public:
    static const std::vector<MaterialData>& load() {
        return DataCache<MaterialData>::load("MaterialDatabase");
    }
    static void setData(std::vector<MaterialData> data) {
        DataCache<MaterialData>::setData(std::move(data));
    }
};