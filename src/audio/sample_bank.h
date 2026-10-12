#pragma once
#include "fail_cache.h"

namespace gpp::audio {
// One upload shared by all BASS voices. Move-only and device-owner scoped.
class PreparedSample {
    HSAMPLE sample_ = 0;

  public:
    PreparedSample() = default;
    PreparedSample(const PcmBuffer &pcm, unsigned voices, bool steal = false);
    ~PreparedSample();
    PreparedSample(PreparedSample &&other) noexcept;
    PreparedSample &operator=(PreparedSample &&other) noexcept;
    PreparedSample(const PreparedSample &) = delete;
    PreparedSample &operator=(const PreparedSample &) = delete;
    HSAMPLE handle() const { return sample_; }
};
struct BankOptions {
    unsigned voices = 32;
    bool stealOldest = false;
};
struct BankStats {
    uint64_t triggers = 0, accepted = 0, rejected = 0, replaced = 0,
             failures = 0;
    unsigned active = 0, peakActive = 0;
    double prepareMs = 0, lastTriggerMs = 0;
    uint64_t pcmBytes = 0;
};
// Exclusively owned by the preparing worker, then by the control thread.
class PreparedFailBank {
    struct Voice {
        DWORD channel = 0;
        uint64_t serial = 0;
    };
    std::shared_ptr<const SongFailCache> cache_;
    std::vector<PreparedSample> samples_;
    std::vector<Voice> voices_;
    BankOptions options_;
    BankStats stats_;
    uint64_t serial_ = 0;
    void collect();

  public:
    PreparedFailBank(std::shared_ptr<const SongFailCache> cache,
                     BankOptions options = {},
                     const Checkpoint &checkpoint = {});
    ~PreparedFailBank();
    PreparedFailBank(const PreparedFailBank &) = delete;
    PreparedFailBank &operator=(const PreparedFailBank &) = delete;
    bool trigger(double songSeconds, unsigned mask, float volume = .7f);
    void stop();
    BankStats stats();
    void resetStatistics();
    const SongFailCache &cache() const { return *cache_; }
};
// Five independently held frets for the selected region. AudioTransport owns
// the existing mixtime-loop callbacks and immutable backing memory.
class SustainedFailPreview {
    std::array<AudioTransport, 5> streams_;

  public:
    SustainedFailPreview(const AnalysisResult &analysis, Settings settings,
                         const Checkpoint &checkpoint = {});
    void press(unsigned fret, float volume = .14f);
    void release(unsigned fret, unsigned milliseconds = 20);
    DWORD handle(unsigned fret) const;
    void stop();
};
} // namespace gpp::audio
