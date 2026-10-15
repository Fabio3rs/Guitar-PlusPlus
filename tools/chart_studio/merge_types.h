#pragma once
// Included after Part: composition is studio-only and does not alter Event's
// historical positional serialization.
struct MergeRegion {
    int64_t begin = 0, end = 0;
    int source = 0; // 0 primary, 1 secondary, 2 silence
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(begin), CEREAL_NVP(end), CEREAL_NVP(source));
    }
};
struct MergeConfig {
    int secondaryTrack = -1, secondaryChannel = -1, secondaryProgram = -1;
    std::string minimum = "1/2", maximum = "off", clearance = "0",
                grid = "1/16";
    std::vector<MergeRegion> regions;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(secondaryTrack), CEREAL_NVP(secondaryChannel),
          CEREAL_NVP(secondaryProgram), CEREAL_NVP(minimum),
          CEREAL_NVP(maximum), CEREAL_NVP(clearance), CEREAL_NVP(grid),
          CEREAL_NVP(regions));
    }
};
struct MergeDecision {
    uint64_t id = 0, sourceId = 0;
    int source = 0;
    int64_t originalDuration = 0, playableDuration = 0, duration = 0, tick = 0;
    bool copied = false;
    std::string reason;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(id), CEREAL_NVP(sourceId), CEREAL_NVP(source),
          CEREAL_NVP(originalDuration), CEREAL_NVP(playableDuration),
          CEREAL_NVP(duration), CEREAL_NVP(tick), CEREAL_NVP(copied),
          CEREAL_NVP(reason));
    }
};
struct MergeDocument {
    MergeConfig config;
    bool initialized = false;
    Part primary, secondary;
    std::map<std::string, uint64_t> ids;
    std::vector<MergeRegion> regions;
    std::vector<MergeDecision> decisions;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(config), CEREAL_NVP(initialized), CEREAL_NVP(primary),
          CEREAL_NVP(secondary), CEREAL_NVP(ids), CEREAL_NVP(regions),
          CEREAL_NVP(decisions));
    }
};
