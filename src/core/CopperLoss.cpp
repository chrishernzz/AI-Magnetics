#include "CopperLoss.h"

// precondition: dcrOhms is a real evaluated DCR (not a placeholder)
// postcondition: returns DC copper loss in watts
double calculateCopperLoss(double rmsCurrentA, double dcrOhms) {
    return rmsCurrentA * rmsCurrentA * dcrOhms;
}
