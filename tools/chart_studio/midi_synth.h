#pragma once
#include "core.h"
#include <atomic>
namespace ChartStudio {
struct SynthEvent {
    int64_t sample;
    int channel, status, a, b;
    size_t order = 0;
};
struct SynthPlan {
    std::vector<SynthEvent> events;
    std::vector<bool> drums;
    int64_t frames = 0;
};
bool synthAvailable();
std::vector<SynthStem> synthSelection(const State &state,
                                      const std::string &mode);
SynthPlan synthPlan(const State &state, const SynthStem &stem,
                    const SynthConfig &config);
void renderStems(const State &state, const SynthConfig &config,
                 const std::function<void()> &checkpoint = {},
                 const std::function<void(double)> &progress = {});
int synthCli(int argc, char **argv);
} // namespace ChartStudio
