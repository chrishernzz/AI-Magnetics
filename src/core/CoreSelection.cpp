#include "CoreSelection.h"

#include "../data/CoreDatabase.h"

//function for core area product to tell us whether a magnetic core
//has enough area to carry the magnetic flux and window area to fit.
static double coreAreaProductCm4(double aeMm2, double waMm2) {
    // Ap = Ae × Aw
    return (aeMm2 * waMm2) * 1e-4;
}

//function to estimate copper loss (simplified: based on core size proxy).
static double estimateCopperLossW(const CoreData& core, double peakCurrentA) {
    // Rough heuristic: larger core = less loss
    // Loss ~ I² × (core_size)^(-0.5)
    double coreSize = core.ae * core.wa;

    // Simplified
    return (peakCurrentA * peakCurrentA) / (coreSize * 0.01);
}

CoreSelectionResult selectCore(const CoreSelectionInput& input) {
    //reference so you do not copy the data
    const auto& cores = CoreDatabase::load();

    //if empty, then return nothing
    if (cores.empty()) {
        return {"Unknown", "Unknown", 0, 0, 0, 0, 0};
    }

    std::cout << "\n=== CORE SELECTION DEBUG ===" << std::endl;
    std::cout << "Input Ap requirement: " << input.areaProduct << " cm⁴" << std::endl;

    std::cout << "Recommended material: " << input.recommendedMaterial << std::endl;

    //Step 1: Filter cores by Ap requirement (with 5% safety margin)
    std::vector<const CoreData*> candidates;

    for (const auto& core : cores) {
        double coreAp = coreAreaProductCm4(core.ae, core.wa);

        //this returns true if the core meets the area product requirement with a 5% margin
        bool passes = (coreAp >= input.areaProduct * 0.95);
        std::cout << "Core " << core.partNumber << " (" << core.material << "): Ap=" << coreAp << " -> " << (passes ? "PASS" : "FAIL");

        if (passes) {
            candidates.push_back(&core);
        }
        std::cout << std::endl;
    }

    std::cout << "Total candidates: " << candidates.size() << std::endl;

    //if no candidates meet Ap, fallback to largest core
    if (candidates.empty()) {
        std::cout << "WARNING: No core meets Ap requirements. " << "Using largest available core." << std::endl;

        const CoreData* largestCore = &cores[0];
        for (const auto& core : cores) {
            if (coreAreaProductCm4(core.ae, core.wa) > coreAreaProductCm4(largestCore->ae, largestCore->wa)) {
                largestCore = &core;
            }
        }

        return {largestCore->partNumber, largestCore->material, largestCore->mu, largestCore->al, largestCore->ae, largestCore->wa, largestCore->le};
    }

    //Step 2: Prefer cores matching recommended material
    std::vector<const CoreData*> materialMatches;

    for (const auto* core : candidates) {
        if (core->material == input.recommendedMaterial) {
            materialMatches.push_back(core);
        }
    }

    //Using material-matched cores if available,
    //otherwise use all candidates
    std::vector<const CoreData*>& finalCandidates = materialMatches.empty() ? candidates : materialMatches;
    std::cout << "\nFiltered by material '" << input.recommendedMaterial << "': " << finalCandidates.size() << " cores" << std::endl;

    //Step 3: Pick best by loss
    const CoreData* bestCore = finalCandidates[0];

    double bestLoss = estimateCopperLossW(*finalCandidates[0], input.peakCurrentA);

    std::cout << "\n-------------------- Loss comparison:" << std::endl;
    std::cout << " " << finalCandidates[0]->partNumber << ": loss=" << bestLoss << " (init)" << std::endl;

    for (const auto* core : finalCandidates) {
        double loss = estimateCopperLossW(*core, input.peakCurrentA);

        std::cout << " "
                  << core->partNumber
                  << ": loss="
                  << loss;

        if (loss < bestLoss) {
            std::cout << " <- NEW BEST";

            bestLoss = loss;
            bestCore = core;
        }

        std::cout << std::endl;
    }

    std::cout << "\nSelected: "
              << bestCore->partNumber
              << " (material="
              << bestCore->material
              << ", loss="
              << bestLoss
              << ")"
              << std::endl;

    std::cout << "========================\n"
              << std::endl;

    return {
        bestCore->partNumber,
        bestCore->material,
        bestCore->mu,
        bestCore->al,
        bestCore->ae,
        bestCore->wa,
        bestCore->le
    };
}