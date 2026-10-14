#pragma once
#include "core.h"
#include <functional>

namespace ChartStudio {
struct ChromaFrame {
    double time = 0;
    std::array<double, 12> chroma{};
};
using ChromaSequence = std::vector<ChromaFrame>;
struct AlignmentSettings {
    std::string mode = "auto";
    double minimumOffset = -15, maximumOffset = 30;
    double minimumScale = .97, maximumScale = 1.03;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(mode), CEREAL_NVP(minimumOffset),
          CEREAL_NVP(maximumOffset), CEREAL_NVP(minimumScale),
          CEREAL_NVP(maximumScale));
    }
    void validate() const;
};
struct AlignmentCandidate {
    double scale = 1, offset = 0, score = 0, coverage = 0;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(scale), CEREAL_NVP(offset), CEREAL_NVP(score),
          CEREAL_NVP(coverage));
    }
};
struct AlignmentWindow {
    double midiTime = 0, residual = 0, score = 0;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(midiTime), CEREAL_NVP(residual), CEREAL_NVP(score));
    }
};
struct AlignmentResult {
    int version = 1;
    std::string midiHash, audioHash, audioPath;
    AlignmentSettings settings;
    std::vector<AlignmentCandidate> candidates;
    std::vector<AlignmentWindow> windows;
    bool reliable = false;
    double margin = 0, suggestedSilence = 0, firstMusicalTime = 0;
    std::string diagnostic;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(version), CEREAL_NVP(midiHash), CEREAL_NVP(audioHash),
          CEREAL_NVP(audioPath), CEREAL_NVP(settings), CEREAL_NVP(candidates),
          CEREAL_NVP(windows), CEREAL_NVP(reliable), CEREAL_NVP(margin),
          CEREAL_NVP(suggestedSilence), CEREAL_NVP(firstMusicalTime),
          CEREAL_NVP(diagnostic));
    }
};
ChromaSequence scoreChroma(const Score &score, double hop = .05);
AlignmentResult alignChroma(const ChromaSequence &midi,
                            const ChromaSequence &audio,
                            const AlignmentSettings &settings,
                            const std::function<void()> &checkpoint = {});
std::string alignmentJson(const AlignmentResult &result);
AlignmentResult decodeAlignment(const std::string &json);
double alignmentSilence(const AlignmentResult &result, size_t candidate);
AlignmentResult readAlignment(const fs::path &path);
// BASS must be initialized on the calling thread. This adapter lives outside
// the CPU library and uses gpp_audio's scanner, without region segmentation.
AlignmentResult analyzeAlignment(const State &state, const fs::path &audio,
                                 const AlignmentSettings &settings,
                                 const std::function<void()> &checkpoint = {});
void applyAlignment(Session &session, const AlignmentResult &result,
                    size_t candidate);
} // namespace ChartStudio
