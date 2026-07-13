#include "AreaProduct.h"

//precondition: none
//postcondition: returns the Energy Max Formula so we can see how much mJ it needs
double calculateStoredEnergy(double inductanceH, double peakCurrentA) {
    //Energy max formula: 1/2 x L x Ipk^2
    return 0.5 * inductanceH * peakCurrentA * peakCurrentA;
}

//precondition: none
//postcondition: returns whether a core is physcially large enough for the required power, current, energy storage, and winding space
double calculateAp(const AreaProductInput& in) {
    //Ap formula for an Energy Storing Inductor: Ap = 2Emax x 10^4 / KuBmaxJ
    double energy = calculateStoredEnergy(in.inductanceH, in.peakCurrentA);
    return (2.0 * energy * 1e4) / (in.windowUtilization * in.fluxDensityT * in.currentDensityAPerCm2);
}