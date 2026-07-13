#pragma once
#include <vector>
#include <string>

//this is our data that core holds from the database 'cores.csv'
struct CoreData {
    std::string partNumber;
    std::string material;
    double mu;
    double al;
    double ae;
    double wa;
    double le;
};

class CoreDatabase{
public: 
    static std::vector<CoreData> load();
};