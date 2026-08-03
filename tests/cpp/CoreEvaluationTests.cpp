#include <cassert>
#include <cstdio>
#include <unordered_map>
#include "TestHelpers.h"
#include "core/sizing/CoreEvaluation.h"
#include "data/CoreDatabase.h"

namespace {

//Illustrative/synthetic ferrite geometry (same numbers other test files use as their synthetic-ferrite
//fixture - originally the real E100/60/28-3C90 catalog row, since removed from data/real_cores.csv now that
//the database is scoped to Magnetics powder cores only, which have no ferrite rows left at all) - large
//enough Ae/Wa that a modest Ap requirement is easy to satisfy, small enough that a huge one is easy to fail,
//so both directions of the ferrite gate are exercised deterministically.
CoreData syntheticFerriteCore() {
    CoreData core;
    core.partNumber = "SYNTHETIC-FERRITE-3C90";
    core.material = "3C90";
    core.mu = 2249.28;
    core.al = 7584.855918773515;
    core.ae = 735.0502256509033;
    core.wa = 2020.0;
    core.le = 273.919572699267;
    core.materialType = "ferrite";
    core.coreShape = "TwoPieceSet";
    return core;
}

//real powder geometry (C055439A2X2/MPP 60, same core GapToleranceTests.cpp's DC-bias tests use) - Ap is
//tiny relative to its Ae/Wa (449.5mm2 x 419.8mm2) versus a deliberately huge required Ap below, so if this
//material still meets the pre-filter, it can only be because materialType=="powder" bypassed the check, not
//because it happened to be big enough.
CoreData realPowderCore() {
    CoreData core;
    core.partNumber = "C055439A2X2";
    core.material = "MPP 60";
    core.mu = 60.0;
    core.al = 316.9118489299326;
    core.ae = 449.4919;
    core.wa = 419.82233603275705;
    core.le = 106.94106558574245;
    core.materialType = "powder";
    core.coreShape = "Toroid";
    return core;
}

MaterialCandidate materialFor(const std::string& name) {
    MaterialCandidate material;
    material.materialFamily = name;
    material.frequencySuitable = true;
    return material;
}

}  //namespace

//1. A ferrite core with a required Ap far beyond what its real geometry provides must still be correctly
//rejected by the pre-filter - the powder bypass below must not have accidentally widened to ferrite too.
void testFerriteCoreStillGatedByAreaProduct() {
    CoreDatabase::setData({syntheticFerriteCore()});
    std::vector<MaterialCandidate> materials{materialFor("3C90")};
    //real Ap for this core is ~14.85 cm^4 (735.05mm2 * 2020mm2 * 1e-4) - require far more than that.
    std::unordered_map<std::string, double> required{{"3C90", 1000.0}};

    std::vector<CoreCandidate> cores = findSuitableCores(materials, required);
    assert(cores.size() == 1);
    assert(!cores[0].meetsAreaProduct);
    std::printf("testFerriteCoreStillGatedByAreaProduct: meetsAreaProduct=false as expected (real Ap << required)\n");
}

//2. A ferrite core with an easily-satisfied Ap requirement must still pass - the powder bypass must not have
//broken the normal ferrite path in the other direction either.
void testFerriteCorePassesWhenRealAreaProductSuffices() {
    CoreDatabase::setData({syntheticFerriteCore()});
    std::vector<MaterialCandidate> materials{materialFor("3C90")};
    std::unordered_map<std::string, double> required{{"3C90", 1.0}};  //trivially small vs real ~14.85 cm^4

    std::vector<CoreCandidate> cores = findSuitableCores(materials, required);
    assert(cores.size() == 1);
    assert(cores[0].meetsAreaProduct);
    std::printf("testFerriteCorePassesWhenRealAreaProductSuffices: meetsAreaProduct=true as expected\n");
}

//3. Regression: a powder core must meet the area-product pre-filter unconditionally, even against a
//deliberately impossible requirement - the McLyman Ap formula this pre-filter uses has no notion of DC-bias
//roll-off, so it must never be allowed to exclude a powder core before the real turns/gap/roll-off solve in
//TurnsAndGapDesign.cpp gets a chance to run. A real user report found 61 of 80 powder cores in the database
//never reaching real evaluation because of this.
void testPowderCoreBypassesAreaProductPreFilter() {
    CoreDatabase::setData({realPowderCore()});
    std::vector<MaterialCandidate> materials{materialFor("MPP 60")};
    //deliberately astronomical - no real core would ever satisfy this via the raw Ap formula.
    std::unordered_map<std::string, double> required{{"MPP 60", 1.0e9}};

    std::vector<CoreCandidate> cores = findSuitableCores(materials, required);
    assert(cores.size() == 1);
    assert(cores[0].meetsAreaProduct);
    std::printf("testPowderCoreBypassesAreaProductPreFilter: meetsAreaProduct=true despite an impossible required Ap (1e9 cm^4)\n");
}

//4. A mixed database (one ferrite, one powder core, same impossible required Ap for both) must apply the two
//different rules independently - proves the bypass is keyed off materialType per-core, not some global flag.
void testMixedDatabaseAppliesRulesIndependently() {
    CoreDatabase::setData({syntheticFerriteCore(), realPowderCore()});
    std::vector<MaterialCandidate> materials{materialFor("3C90"), materialFor("MPP 60")};
    std::unordered_map<std::string, double> required{{"3C90", 1.0e9}, {"MPP 60", 1.0e9}};

    std::vector<CoreCandidate> cores = findSuitableCores(materials, required);
    assert(cores.size() == 2);
    for (const auto& c : cores) {
        if (c.materialType == "ferrite") {
            assert(!c.meetsAreaProduct);
        } else if (c.materialType == "powder") {
            assert(c.meetsAreaProduct);
        } else {
            assert(false && "unexpected materialType in mixed-database test");
        }
    }
    std::printf("testMixedDatabaseAppliesRulesIndependently: ferrite still gated, powder still bypassed, same request\n");
}

void runCoreEvaluationTests() {
    testFerriteCoreStillGatedByAreaProduct();
    testFerriteCorePassesWhenRealAreaProductSuffices();
    testPowderCoreBypassesAreaProductPreFilter();
    testMixedDatabaseAppliesRulesIndependently();
    std::printf("All CoreEvaluationTests passed.\n");
}
