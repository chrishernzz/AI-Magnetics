#pragma once

// STAGE 6c: High-Frequency Losses (skin/proximity effect)
//
// SPLIT OUT from LossAnalysis.cpp — see CopperLoss.h for why.
// Uses currentWaveform shape (an input carried from AreaProduct stage)
// since skin/proximity effect depends on harmonic content, not just RMS.

double calculateHighFrequencyLoss(double switchingFreqHz, double wireDiameterCm);
