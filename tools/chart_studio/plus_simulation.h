#pragma once
#include "PlusRules.h"
#include "plus_generator.h"

namespace ChartStudio {
struct PlusSimulationSettings {
    uint64_t seed = 42;
    int trials = 256;
    double success = 1, reserve = .4, initialEnergy = 0;
    std::string strategy = "dense";
    void validate() const;
};
struct PlusEnergyPoint {
    double time = 0, energy = 0;
    bool active = false;
};
struct PlusActivation {
    double begin = 0, end = 0;
};
struct PlusSimulation {
    double completed = 0, lost = 0, wasted = 0, activeSeconds = 0, benefit = 0,
           availableRegions = 0, finalEnergy = 0;
    std::vector<double> regionEnergy;
    // Representative trial 0, not an average that could be mistaken for a
    // playthrough.
    std::vector<PlusEnergyPoint> trajectory;
    std::vector<PlusActivation> activations;
};
PlusSimulation simulatePlus(const PlusAnalysis &analysis,
                            const std::vector<PlusPhrase> &phrases,
                            const PlusSimulationSettings &settings,
                            const std::function<void()> &checkpoint = {});
std::string plusReport(const PlusAnalysis &analysis,
                       const PlusDocument &document, int trials = 256,
                       uint64_t seed = 42,
                       const std::function<void()> &checkpoint = {});
std::string plusReport(const State &state, int trials = 256, uint64_t seed = 42,
                       const std::function<void()> &checkpoint = {});
} // namespace ChartStudio
