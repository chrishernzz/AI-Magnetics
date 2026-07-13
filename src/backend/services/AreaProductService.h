#pragma once
#include "../../core/AreaProduct.h"

//this is the Response that the user will get (the data they are recieving back)
struct AreaProductResult {
    double energy;
    double areaProduct;
};

class AreaProductService {
public:
    AreaProductResult calculate(const AreaProductInput& input);
};