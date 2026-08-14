#pragma once
#include <optional>
#include <string>

/*
How much to trust a value: Manufacturer (from a real datasheet/vendor
record), Measured (real bench data), Derived (computed from other real
data), Estimated (a reasonable engineering guess), Phase1Default (a
documented DesignRules constant, not per-part data at all).
*/
enum class DataConfidence { Manufacturer, Measured, Derived, Estimated, Phase1Default };

/*
Provenance for one material or core candidate's data. datasheetRevision,
datasheetUrl, and dateAccessed are always std::nullopt in Phase 1 - no
real revision/URL/access-date exists anywhere in the current data
snapshot to populate them with, and inventing one would violate the
project's "never invent values silently" rule. This struct exists so the
schema can carry that information once it is genuinely sourced, not as a
claim that it already has been.
*/
struct SourceInfo {
    std::optional<std::string> manufacturer;
    std::optional<std::string> partNumber;
    std::optional<std::string> materialGrade;
    std::optional<std::string> datasheetName;
    std::optional<std::string> datasheetRevision;
    std::optional<std::string> datasheetUrl;
    std::optional<std::string> dateAccessed;
    DataConfidence confidence = DataConfidence::Phase1Default;
    std::string confidenceNote;
};
