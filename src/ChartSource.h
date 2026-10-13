#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Import evidence only: never serialized into historical .gpp archives.
// Owned by the loaded chart, copied/moved together with its note buffers.
struct ChartSourceInfo {
    struct Tempo {
        int64_t tick = 0;
        double seconds = 0.0;
        double bpm = 120.0;
    };
    struct Event {
        int64_t tick = 0;
        double seconds = 0.0;
        std::string track, kind, text;
    };
    struct Gem {
        int64_t tick = 0, endTick = 0;
        int lane = 0;
        double seconds = 0.0, duration = 0.0;
    };

    int ppq = 0;
    int hopoThresholdTicks = 0;
    double offset = 0.0;
    std::string hopoPolicy;
    std::vector<Tempo> tempos;
    std::vector<Event> events;
    std::vector<Gem> gems;
    std::vector<int64_t> noteTicks;
    std::vector<std::string> hopoReasons;
    std::vector<std::string> warnings;
};
