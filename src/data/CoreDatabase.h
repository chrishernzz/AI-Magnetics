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

    //Mean-length-per-turn (mm), estimated from real core column geometry (see scripts/export_real_data.py) - 0.0 means "no data", callers must treat that as missing, not as a real zero-length core.
    double mlt = 0.0;
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