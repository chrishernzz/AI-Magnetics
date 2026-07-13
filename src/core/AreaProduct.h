#pragma once

//STAGE 2: Calculate Ap (Area Product)

/*
In the buck-inductor scope, L is a GIVEN input from the circuit designer, and Ap is the value
that actually drives core sizing.

Inputs (the 5 values your boss specified):
L               - inductance (given)
Ipk             - peak current (given)
currentWaveform - shape of the current (given)
switchingFreqHz - switching frequency (given)
allowableTempRiseC - allowable temperature rise (given)

//Output: Ap, the area product used to enter the core selection tables.

*/

//data that the user needs to enter
struct AreaProductInput {
    double inductanceH;
    double peakCurrentA;
    double switchingFreqHz;
    double allowableTempRiseC;

    // Ku = Window utilization factor
    double windowUtilization;
    // Bm = maximum flun density
    double fluxDensityT;
    // J = current density
    double currentDensityAPerCm2;
};

//function gets Energy
double calculateStoredEnergy(double inductanceH, double peakCurrentA);
double calculateAp(const AreaProductInput& in);
