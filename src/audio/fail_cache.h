#pragma once
#include "song_analysis.h"

namespace gpp::audio {
struct FailSample {
    PcmBuffer pcm;
    double contentSeconds = 0;
};
struct FailSet {
    std::array<uint32_t, 5> sampleIds{};
    uint64_t representative = 0;
    AnalysisResult analysis;
    bool fallback = false;
};
struct SongFailCache {
    SongAnalysis song;
    Settings settings;
    uint32_t fxVersion = 0;
    std::vector<FailSet> sets;
    std::vector<FailSample> samples;
    double representativeMs = 0, synthMs = 0, fxMs = 0;
    const FailRegion *regionAt(double seconds) const;
    uint64_t pcmBytes() const;
};
Settings shortFailSettings();
SongFailCache generateFailCache(const SongAnalysis &song,
                                const Settings &settings,
                                const Checkpoint &checkpoint = {});
void validate(const SongFailCache &cache);
bool sourceMatches(const SourceIdentity &identity,
                   const std::filesystem::path &path,
                   const Checkpoint &checkpoint = {});
void saveSongAnalysis(const std::filesystem::path &path,
                      const SongAnalysis &song,
                      const Checkpoint &checkpoint = {});
SongAnalysis loadSongAnalysis(const std::filesystem::path &path,
                              const std::filesystem::path &source,
                              const Checkpoint &checkpoint = {});
void saveFailCache(const std::filesystem::path &path,
                   const SongFailCache &cache,
                   const Checkpoint &checkpoint = {});
SongFailCache loadFailCache(const std::filesystem::path &path,
                            const std::filesystem::path &source,
                            const Checkpoint &checkpoint = {});
// JSON is an interchange artifact; the core interfaces above remain typed.
std::string songReportJson(const SongAnalysis &song, bool observations = false);
std::string cacheReportJson(const SongFailCache &cache);
} // namespace gpp::audio
