#pragma once
#include "audio.h"

namespace gpp::audio {
struct SongSettings {
    int fft = 8192;
    double hop = .1, smoothing = .6, persistence = .8, minimum = 4;
    double threshold = .25, hysteresis = .15, harmonicWeight = .6;
    double silenceDb = -60, silenceExitDb = -54;
    double merge = .10, dedupTolerance = .10;
    bool deduplicate = true, fixed = false;
};
template <class Visitor>
void visitSongSettings(SongSettings &s, Visitor visit) {
#define FIELD(name, lo, hi) visit("song." #name, s.name, double(lo), double(hi))
    FIELD(fft, 512, 32768);
    FIELD(hop, .01, 5);
    FIELD(smoothing, .01, 10);
    FIELD(persistence, .01, 10);
    FIELD(minimum, 0, 120);
    FIELD(threshold, .001, 1);
    FIELD(hysteresis, 0, 1);
    FIELD(harmonicWeight, 0, 1);
    FIELD(silenceDb, -120, 0);
    FIELD(silenceExitDb, -120, 0);
    FIELD(merge, 0, 1);
    FIELD(dedupTolerance, 0, 1);
    FIELD(deduplicate, 0, 1);
    FIELD(fixed, 0, 1);
#undef FIELD
}
void validate(const SongSettings &s);
struct Signature {
    std::array<float, 12> chroma{};
    std::array<float, 24> bands{};
};
double distance(const Signature &a, const Signature &b, double harmonicWeight);
struct Observation {
    uint64_t frame = 0;
    Signature signature;
    double rms = 0, variation = 0;
};
struct FailRegion {
    uint64_t begin = 0, end = 0, representative = 0;
    Signature signature;
    uint32_t setId = 0;
    bool silent = false;
    double change = 0;
};
struct SourceIdentity {
    std::string path, hash;
    uint64_t bytes = 0;
};
SourceIdentity identifySource(const std::filesystem::path &path,
                              const Checkpoint &checkpoint = {});
struct SongAnalysis {
    SourceIdentity source;
    SourceInfo info;
    uint64_t frames = 0;
    uint32_t bassVersion = 0;
    SongSettings settings;
    std::vector<Observation> observations;
    std::vector<FailRegion> regions;
    size_t detectedRegions = 0;
    double identifyMs = 0, openMs = 0, decodeMs = 0, fftMs = 0;
    double descriptorMs = 0, scanMs = 0, segmentMs = 0;
};
SongAnalysis scanSong(const std::filesystem::path &source,
                      const SongSettings &settings,
                      const Checkpoint &checkpoint = {});
SongAnalysis segmentSong(const SongAnalysis &scan, const SongSettings &settings,
                         const Checkpoint &checkpoint = {});
void validate(const SongAnalysis &analysis);
} // namespace gpp::audio
