#pragma once
#include <string>
#include <vector>
#include "DataCache.h"
//this is our data that holds from the database 'dc_bias_curves.csv'
struct DCBiasCurveData {
    //matches CoreData::material / MaterialData::name exactly (e.g. "MPP 60", "Kool Mu 60") - this is how
    //findDCBiasCurve() (see PermeabilityRolloff.h) looks up the row for a given powder core candidate.
    std::string materialName;

    //curve-fit coefficients for %initial_permeability = 1/(a + b*H^c) + d, H in Oersteds - real,
    //manufacturer-published values (Magnetics Inc./Micrometals "Permeability vs. DC Bias" datasheet pages,
    //Toroid shape family), not derived or fitted by this project. d is 0.0 for every Magnetics Inc. material
    //currently in this dataset (their fit formula omits the +d term entirely) and a real nonzero-capable term
    //only for Micrometals' formula (still 0.0 for Mix-26 specifically, but the field exists so a future
    //material with a real nonzero d isn't silently dropped).
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    double d = 0.0;

    //real vendor/manufacturer name and datasheet URL this curve was read from - same provenance convention
    //as MaterialData::manufacturer/datasheetUrl. Empty string means genuinely not recorded, never a placeholder.
    std::string vendor;
    std::string datasheetUrl;
};

class DCBiasCurveDatabase {
public:
    static const std::vector<DCBiasCurveData>& load() {
        return DataCache<DCBiasCurveData>::load("DCBiasCurveDatabase");
    }
    static void setData(std::vector<DCBiasCurveData> data) {
        DataCache<DCBiasCurveData>::setData(std::move(data));
    }
};
