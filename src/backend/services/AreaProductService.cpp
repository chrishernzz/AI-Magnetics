#include "AreaProductService.h"

//precondition: calls AreaProductInput and only get inductance and peak current
//postcondition: returns the area product result and shows energy and Ap to the user
AreaProductResult AreaProductService::calculate(const AreaProductInput& input) {
    AreaProductResult result;

    //user will see this
    result.energy = calculateStoredEnergy(input.inductanceH,input.peakCurrentA);
    result.areaProduct = calculateAp(input);

    return result;
}