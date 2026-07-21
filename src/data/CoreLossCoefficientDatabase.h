#pragma once
#include <vector>
#include <string>
#include "DataCache.h"
//this is our data that core holds from the database 'real_core_loss_coefficients.csv'
struct CoreLossCoefficientData {
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

class CoreLossCoefficientDatabase{
public:
    static const std::vector<CoreLossCoefficientData>& load() {
        return DataCache<CoreLossCoefficientData>::load("CoreLossCoefficientDatabase");
    }
    static void setData(std::vector<CoreLossCoefficientData> data){
        DataCache<CoreLossCoefficientData>::setData(std::move(data));
    }
};
