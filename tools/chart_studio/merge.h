#pragma once
#include "core.h"

namespace ChartStudio {
struct MergeProposal {
    std::string source;
    State state;
};
// Fractions of a whole note. Comparisons use rational arithmetic; grid
// positions round to nearest tick, ties toward the earlier tick.
int64_t mergeSnap(int64_t tick, int ppq, const std::string &division);
void validateMerge(const State &state, const MergeConfig &config);
MergeProposal proposeMerge(const State &state, const MergeConfig &config,
                           const std::vector<Pattern> &library = {},
                           bool regenerateSources = false);
void generateMerged(State &state, const std::vector<Pattern> &library);
std::string mergeReport(const State &state);
std::string mergeRecipe(const State &state);
void readMergeRecipe(State &state, const std::string &text);
} // namespace ChartStudio
