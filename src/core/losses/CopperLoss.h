#pragma once

// STAGE 8a: DC copper loss
//
// Pcu_dc = Irms^2 * DCR. Uses RMS current, never peak current (spec
// section 12) - winding heating depends on the current's RMS value, not
// its peak. DCR must already reflect the winding temperature assumption
// used to compute it (see WindingDesign) - this function does not apply
// any further temperature correction itself.
double calculateCopperLoss(double rmsCurrentA, double dcrOhms);
