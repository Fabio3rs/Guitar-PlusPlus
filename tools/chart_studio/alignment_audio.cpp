#include "alignment.h"
#include "audio/song_analysis.h"
#include <stdexcept>

namespace ChartStudio {
AlignmentResult analyzeAlignment(const State &state, const fs::path &audio,
                                 const AlignmentSettings &settings,
                                 const std::function<void()> &checkpoint) {
    settings.validate();
    const auto identity = fileHash(audio);
    const auto midi = scoreChroma(state.score);
    gpp::audio::SongSettings scanSettings;
    scanSettings.hop = .05;
    const auto scan = gpp::audio::scanSong(audio, scanSettings, [&](double) {
        if (checkpoint) {
            checkpoint();
        }
    });
    ChromaSequence recording;
    recording.reserve(scan.observations.size());
    for (const auto &o : scan.observations) {
        ChromaFrame f;
        // Scanner labels the window's first sample; Hann FFT energy is
        // centered.
        f.time = (double(o.frame) + scanSettings.fft * .5) / scan.info.rate;
        if (o.rms > 1e-4) {
            for (size_t j = 0; j < 12; ++j) {
                f.chroma[j] = o.signature.chroma[j];
            }
        }
        recording.push_back(f);
    }
    auto result = alignChroma(midi, recording, settings, checkpoint);
    result.midiHash = state.inputHash;
    result.audioHash = fileHash(audio);
    if (result.audioHash != identity) {
        throw std::runtime_error(
            "Audio changed during analysis; analyze again");
    }
    result.audioPath = fs::absolute(audio).u8string();
    return result;
}
} // namespace ChartStudio
