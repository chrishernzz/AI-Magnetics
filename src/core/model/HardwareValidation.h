#pragma once
#include <optional>
#include <string>

// A place to record real bench-measurement data against a candidate's
// predicted values, once such measurements exist. Every field is
// std::optional and status defaults to "not_measured" - this struct is
// schema only in Phase 1, populated with nothing. No measured value is
// ever fabricated to fill it in.
struct HardwareValidationRecord {
    std::optional<double> measuredInductanceUH;
    std::optional<double> measuredDcrMilliOhm;
    std::optional<double> measuredRippleCurrentA;
    std::optional<double> measuredPeakCurrentA;
    std::optional<double> measuredCoreTempC;
    std::optional<double> measuredWindingTempC;
    std::optional<double> predictedVsMeasuredInductanceErrorPercent;
    std::optional<double> predictedVsMeasuredDcrErrorPercent;
    std::optional<std::string> testNotes;
    std::optional<std::string> testDate;
    std::string status = "not_measured";
};
