#pragma once

/*

Centralizes every unit-conversion factor used anywhere in the engine, so
exactly one file owns "1e-6", "1000.0", "0.01", etc. rather than each
calculation module re-deriving its own inline magic numbers.

Two families exist because the engine is not uniformly SI:

- Most stages (API boundary, RequirementDerivationService, WindingDesign,
  DesignValidation, LossEvaluation) use SI (H, Hz, m, m^2, m^3, Ohm).
- GapDesign/TurnsAndGapDesign/TurnsCalculation deliberately stay in CGS
  (cm, cm^2, nH) because that's the unit system the McLyman AL formula
  was verified against real catalog data in (see GapDesign.h - ~0.03%
  accuracy against real_cores.csv's E100/60/28-3C90 row). Forcing that
  formula into SI would mean re-deriving and re-validating a formula that
  already has a real numerical accuracy guarantee - out of scope here.

Both families are centralized here so "no calculation module performs its
own inline unit conversion" holds for either unit system.

*/
namespace units {

//---- SI boundary conversions ----
constexpr double uHToH(double v) { return v * 1e-6; }
constexpr double hToUH(double v) { return v * 1e6; }
constexpr double kHzToHz(double v) { return v * 1e3; }
constexpr double hzToKHz(double v) { return v * 1e-3; }
constexpr double mmToM(double v) { return v * 1e-3; }
constexpr double mToMm(double v) { return v * 1e3; }
constexpr double mm2ToM2(double v) { return v * 1e-6; }
constexpr double m2ToMm2(double v) { return v * 1e6; }
constexpr double mm3ToM3(double v) { return v * 1e-9; }
constexpr double m3ToMm3(double v) { return v * 1e9; }
constexpr double milliOhmToOhm(double v) { return v * 1e-3; }
constexpr double ohmToMilliOhm(double v) { return v * 1e3; }
constexpr double aPerCm2ToAPerMm2(double v) { return v / 100.0; }
constexpr double aPerMm2ToAPerCm2(double v) { return v * 100.0; }

//---- CGS conversions (GapDesign/TurnsAndGapDesign/TurnsCalculation domain only) ----
constexpr double mmToCm(double v) { return v * 0.1; }
constexpr double cmToMm(double v) { return v * 10.0; }
constexpr double mm2ToCm2(double v) { return v * 0.01; }
constexpr double cm2ToMm2(double v) { return v * 100.0; }
constexpr double uHToNh(double v) { return v * 1000.0; }
constexpr double nHToUh(double v) { return v * 1e-3; }

}  //namespace units
