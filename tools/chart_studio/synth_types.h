#pragma once
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <string>
#include <vector>
namespace ChartStudio {
struct SynthSource {
    int track = -1, channel = -1, program = -1;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(track), CEREAL_NVP(channel), CEREAL_NVP(program));
    }
};
struct SynthStem {
    std::string name;
    bool enabled = true;
    std::vector<SynthSource> sources;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(name), CEREAL_NVP(enabled), CEREAL_NVP(sources));
    }
};
struct SynthConfig {
    std::string mode = "chart", soundfont, output, format = "wav";
    int sampleRate = 44100, quality = 5;
    double gain = .5, tail = 2;
    std::vector<SynthStem> stems;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(mode), CEREAL_NVP(soundfont), CEREAL_NVP(output),
          CEREAL_NVP(format), CEREAL_NVP(sampleRate), CEREAL_NVP(quality),
          CEREAL_NVP(gain), CEREAL_NVP(tail), CEREAL_NVP(stems));
    }
};
} // namespace ChartStudio
