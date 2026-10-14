#pragma once
#include "core.h"
#include <functional>

namespace ChartStudio {
struct PlusAttack {
    int64_t tick = 0, endTick = 0;
    double time = 0, endTime = 0;
    int mask = 0;
};
struct PlusRegion {
    int64_t begin = 0, end = 0;
    double time = 0, endTime = 0, value = 0;
};
struct PlusAnalysis {
    int ppq = 480;
    std::vector<Tempo> tempos{{}};
    double scale = 1, offset = 0;
    double seconds(int64_t tick) const;
    std::vector<PlusAttack> attacks;
    std::vector<int64_t> bars;
    std::vector<PlusRegion> regions;
    std::vector<std::string> diagnostics;
};
struct PlusProposal {
    std::string source;
    PlusDocument document;
    PlusAnalysis analysis;
    std::vector<std::string> diagnostics;
};
PlusSettings plusProfile(const std::string &name);
// Export-compatible attacks without strategic analysis or measure enumeration.
std::vector<PlusAttack> plusAttacks(const State &state);
PlusAnalysis analyzePlus(const State &state);
std::vector<std::string> plusProblems(const State &state);
PlusProposal proposePlus(const State &state, const PlusSettings &settings,
                         std::optional<std::pair<int64_t, int64_t>> region = {},
                         const std::function<void()> &checkpoint = {});
std::pair<size_t, size_t>
plusAttackRange(const std::vector<PlusAttack> &attacks,
                const PlusPhrase &phrase);
} // namespace ChartStudio
