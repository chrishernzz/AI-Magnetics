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
};

class Materials {
public:
    static const std::vector<MaterialData>& load() {
        return DataCache<MaterialData>::load("Materials");
    }
    static void setData(std::vector<MaterialData> data) {
        DataCache<MaterialData>::setData(std::move(data));
    }
};