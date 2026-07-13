#pragma once
#include <string>
#include <vector>

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
    static std::vector<MaterialData> load();
};