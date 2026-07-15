#pragma once
#include <vector>
#include <string>
#include "DataCache.h"
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
    static const std::vector<CoreData>& load() {
        return DataCache<CoreData>::load("CoreDatabase");
    }
    static void setData(std::vector<CoreData> data){
        DataCache<CoreData>::setData(std::move(data));
    }
};