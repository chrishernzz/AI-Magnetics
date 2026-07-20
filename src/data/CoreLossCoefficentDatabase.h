#pragma once
#include <vector>
#include <string>
#include "DataCache.h"
//this is our data that core holds from the database 'real_core_loss_coefficients.csv'
struct CoreLossCoefficentData {
    std::string materialName;
    double minFreqHz;
    double maxFreqHz; 
    double k;
    double alpha;
    double beta;
    double ct0;
    double ct1;
    double ct2;
};

class CoreLossCoefficentDatabase{
public: 
    static const std::vector<CoreLossCoefficentData>& load() {
        return DataCache<CoreLossCoefficentData>::load("CoreLossCoefficentDatabase");
    }
    static void setData(std::vector<CoreLossCoefficentData> data){
        DataCache<CoreLossCoefficentData>::setData(std::move(data));
    }
};