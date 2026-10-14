#pragma once

// Shared economy only; no gameplay state, UI, or serialization dependencies.
namespace gpp::plus {
inline constexpr double maximumEnergy = 1.0;
inline constexpr double phrasesPerBar = 5.0;
inline constexpr double drainPerSecond = 0.05;
inline constexpr double phraseEnergy = maximumEnergy / phrasesPerBar;
} // namespace gpp::plus
