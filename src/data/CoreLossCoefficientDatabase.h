#pragma once
#include <optional>
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

    //valid flux-density-swing range and reference test temperature this coefficient row was fitted over - always std::nullopt against the current real_core_loss_coefficients.csv, which carries no such columns.
    //CoreLoss.cpp's range check is written to skip when these are absent, so this is a documented no-op today, not a claim that flux-swing extrapolation is currently being guarded against.
    std::optional<double> minFluxSwingT;
    std::optional<double> maxFluxSwingT;
    std::optional<double> testTemperatureC;
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
