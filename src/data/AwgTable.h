#pragma once

//Standard round magnet wire geometry (bare copper diameter/area) for AWG 8 through 30, per NEMA MW1000 - this is public reference geometry, not a
//fabricated or measured design constant, unlike DesignRules' engineering defaults.
struct AwgEntry {
    int awg;
    double diameterMm;
    double areaMm2;
};

inline constexpr AwgEntry kAwgTable[] = {
    {8, 3.264, 8.366},  {9, 2.906, 6.632},  {10, 2.588, 5.261}, {11, 2.305, 4.172},
    {12, 2.053, 3.309}, {13, 1.828, 2.627}, {14, 1.628, 2.081}, {15, 1.450, 1.652},
    {16, 1.291, 1.309}, {17, 1.150, 1.038}, {18, 1.024, 0.823}, {19, 0.912, 0.653},
    {20, 0.812, 0.518}, {21, 0.723, 0.410}, {22, 0.644, 0.326}, {23, 0.573, 0.258},
    {24, 0.511, 0.205}, {25, 0.455, 0.162}, {26, 0.405, 0.129}, {27, 0.361, 0.102},
    {28, 0.321, 0.0810}, {29, 0.286, 0.0642}, {30, 0.255, 0.0509},
};

inline constexpr int kAwgTableSize = sizeof(kAwgTable) / sizeof(kAwgTable[0]);
