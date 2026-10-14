#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ChartStudio {
struct MusicalMarker {
    int64_t tick = 0;
    std::string text;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(tick), CEREAL_NVP(text));
    }
};
struct PlusSettings {
    bool enabled = false;
    std::string profile = "strategic";
    double reserve = .4, spacingBars = 8, structureWeight = 2,
           strategyWeight = 4, riskWeight = 2;
    int preferredBars = 1;
    void validate() const;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(enabled), CEREAL_NVP(profile), CEREAL_NVP(reserve),
          CEREAL_NVP(spacingBars), CEREAL_NVP(structureWeight),
          CEREAL_NVP(strategyWeight), CEREAL_NVP(riskWeight),
          CEREAL_NVP(preferredBars));
    }
};
struct PlusPhrase {
    uint64_t id = 0;
    int64_t begin = 0, end = 0;
    bool manual = false;
    int status = 0; // experimental, confirmed, locked
    double risk = 0;
    // Transient import evidence, never persisted in editable studio projects.
    int64_t runtimeFirst = -1, runtimeLast = -1;
    std::string reason;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(id), CEREAL_NVP(begin), CEREAL_NVP(end),
          CEREAL_NVP(manual), CEREAL_NVP(status), CEREAL_NVP(risk),
          CEREAL_NVP(reason));
    }
};
struct PlusDocument {
    int version = 1;
    PlusSettings settings;
    std::vector<PlusPhrase> phrases;
    uint64_t nextId = 1;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(version), CEREAL_NVP(settings), CEREAL_NVP(phrases),
          CEREAL_NVP(nextId));
    }
};
} // namespace ChartStudio
