#pragma once
#include <string>

// Reproducibility metadata for one run's response, so an old result can be
// checked against the code/data that produced it after either changes.
// coreDatabaseVersion/materialDatabaseVersion are set at FastAPI startup
// (see python/app.py) from a real content hash of the loaded CSV bytes -
// they are NOT upstream PyOpenMagnetics/MAS release version numbers, since
// no such version is tracked or exposed anywhere in this snapshot
// mechanism. The hash is real and reproducible: it changes if and only if
// the underlying data actually changes.
struct EngineVersions {
    std::string calculationEngineVersion = "1.1.0";
    std::string designRulesVersion = "1.1.0";
    std::string coreDatabaseVersion;
    std::string materialDatabaseVersion;
};

// Holds the two database version strings set once at FastAPI startup (see
// python/app.py) so every run's DesignRecommendation.versions can read
// them without threading a parameter through the whole call graph - same
// "load once, serve from memory" spirit as DataCache<T>, just for a single
// pair of strings rather than a vector.
class EngineVersionsStore {
public:
    static void setDatabaseVersions(std::string coreDatabaseVersion, std::string materialDatabaseVersion) {
        coreDatabaseVersion_ = std::move(coreDatabaseVersion);
        materialDatabaseVersion_ = std::move(materialDatabaseVersion);
    }
    static EngineVersions current() {
        EngineVersions v;
        v.coreDatabaseVersion = coreDatabaseVersion_;
        v.materialDatabaseVersion = materialDatabaseVersion_;
        return v;
    }

private:
    // inline (C++17) so this header-only class has no ODR violation across
    // the multiple translation units that include it.
    inline static std::string coreDatabaseVersion_;
    inline static std::string materialDatabaseVersion_;
};
