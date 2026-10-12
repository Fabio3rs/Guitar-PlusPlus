#include "sample_bank.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace gpp::audio {
namespace {
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}
} // namespace
PreparedSample::PreparedSample(const PcmBuffer &pcm, unsigned voices,
                               bool steal) {
    levels(pcm);
    if (pcm.samples.empty() ||
        pcm.samples.size() > UINT32_MAX / sizeof(float) || voices < 1 ||
        voices > 256) {
        throw Error("Invalid prepared sample size/voice limit");
    }
    sample_ = BASS_SampleCreate(
        DWORD(pcm.samples.size() * sizeof(float)), pcm.rate, pcm.channels,
        voices, BASS_SAMPLE_FLOAT | (steal ? BASS_SAMPLE_OVER_POS : 0));
    check(sample_ != 0, "Create prepared sample");
    if (!BASS_SampleSetData(sample_, pcm.samples.data())) {
        const auto error = BASS_ErrorGetCode();
        BASS_SampleFree(sample_);
        sample_ = 0;
        throw Error("Upload prepared sample: BASS error " +
                    std::to_string(error));
    }
}
PreparedSample::~PreparedSample() {
    if (sample_) {
        BASS_SampleFree(sample_);
    }
}
PreparedSample::PreparedSample(PreparedSample &&o) noexcept
    : sample_(std::exchange(o.sample_, 0)) {}
PreparedSample &PreparedSample::operator=(PreparedSample &&o) noexcept {
    if (this != &o) {
        if (sample_) {
            BASS_SampleFree(sample_);
        }
        sample_ = std::exchange(o.sample_, 0);
    }
    return *this;
}

PreparedFailBank::PreparedFailBank(std::shared_ptr<const SongFailCache> cache,
                                   BankOptions options, const Checkpoint &cb)
    : cache_(std::move(cache)), options_(options) {
    const auto start = Clock::now();
    if (!cache_ || options.voices < 1 || options.voices > 256) {
        throw Error("Bank voice limit must be 1..256");
    }
    validate(*cache_);
    if (cache_->settings.synth.loop) {
        throw Error("Use sustained preview for looped effects");
    }
    voices_.resize(options.voices);
    samples_.reserve(cache_->samples.size());
    for (size_t i = 0; i < cache_->samples.size(); ++i) {
        if (cb) {
            cb(double(i) / cache_->samples.size());
        }
        samples_.emplace_back(cache_->samples[i].pcm, options.voices);
    }
    stats_.pcmBytes = cache_->pcmBytes();
    stats_.prepareMs = elapsed(start);
    if (cb) {
        cb(1);
    }
}
PreparedFailBank::~PreparedFailBank() { stop(); }

void PreparedFailBank::collect() {
    stats_.active = 0;
    for (auto &v : voices_) {
        if (v.channel &&
            BASS_ChannelIsActive(v.channel) == BASS_ACTIVE_PLAYING) {
            ++stats_.active;
        } else {
            v.channel = 0;
        }
    }
    stats_.peakActive = std::max(stats_.peakActive, stats_.active);
}

bool PreparedFailBank::trigger(double time, unsigned mask, float volume) {
    if (mask > 31 || !std::isfinite(volume) || volume < 0 || volume > 1) {
        throw Error("Invalid bank fret mask/volume");
    }
    const auto *region = cache_->regionAt(time);
    if (!region) {
        throw Error("Cache not ready for this time");
    }
    if (!mask) {
        return true;
    }
    const auto start = Clock::now();
    ++stats_.triggers;
    collect();
    unsigned needed = 0;
    for (unsigned bit = 0; bit < 5; ++bit) {
        needed += (mask >> bit) & 1;
    }
    if (needed > voices_.size() ||
        (needed > voices_.size() - stats_.active && !options_.stealOldest)) {
        ++stats_.rejected;
        stats_.lastTriggerMs = elapsed(start);
        return false;
    }
    while (needed > voices_.size() - stats_.active) {
        auto oldest = voices_.end();
        for (auto it = voices_.begin(); it != voices_.end(); ++it) {
            if (it->channel &&
                (oldest == voices_.end() || it->serial < oldest->serial)) {
                oldest = it;
            }
        }
        BASS_ChannelStop(oldest->channel);
        oldest->channel = 0;
        --stats_.active;
        ++stats_.replaced;
    }
    std::array<DWORD, 5> acquired{};
    unsigned count = 0;
    try {
        for (unsigned fret = 0; fret < 5; ++fret) {
            if (!(mask & (1u << fret))) {
                continue;
            }
            const auto id = cache_->sets[region->setId].sampleIds[fret];
            const auto channel =
                BASS_SampleGetChannel(samples_[id].handle(), 0);
            check(channel != 0, "Acquire bank voice");
            acquired[count++] = channel;
            check(BASS_ChannelSetAttribute(channel, BASS_ATTRIB_VOL,
                                           volume / needed),
                  "Set bank voice gain");
            check(BASS_ChannelSetPosition(channel, 0, BASS_POS_BYTE),
                  "Restart bank voice");
        }
        for (unsigned i = 0; i < count; ++i) {
            check(BASS_ChannelPlay(acquired[i], FALSE), "Play bank voice");
        }
        for (unsigned i = 0; i < count; ++i) {
            auto slot = std::find_if(voices_.begin(), voices_.end(),
                                     [](const auto &v) { return !v.channel; });
            *slot = {acquired[i], ++serial_};
        }
        ++stats_.accepted;
    } catch (...) {
        for (unsigned i = 0; i < count; ++i) {
            BASS_ChannelStop(acquired[i]);
        }
        ++stats_.failures;
        stats_.lastTriggerMs = elapsed(start);
        throw;
    }
    stats_.lastTriggerMs = elapsed(start);
    collect();
    return true;
}

void PreparedFailBank::stop() {
    for (auto &v : voices_) {
        if (v.channel) {
            BASS_ChannelStop(v.channel);
            v.channel = 0;
        }
    }
    stats_.active = 0;
}
BankStats PreparedFailBank::stats() {
    collect();
    return stats_;
}
void PreparedFailBank::resetStatistics() {
    const auto prepare = stats_.prepareMs;
    const auto bytes = stats_.pcmBytes;
    stats_ = {};
    stats_.prepareMs = prepare;
    stats_.pcmBytes = bytes;
    collect();
}

SustainedFailPreview::SustainedFailPreview(const AnalysisResult &analysis,
                                           Settings settings,
                                           const Checkpoint &cb) {
    settings.synth.loop = true;
    for (unsigned fret = 0; fret < 5; ++fret) {
        auto progress = [&](double p) {
            if (cb) {
                cb((fret + p) / 5);
            }
        };
        settings.synth.fretMask = 1 << fret;
        auto pcm = processFx(synthesize(analysis, settings.synth, progress),
                             settings.fx, progress);
        bakeLoop(pcm, settings.synth.crossfade);
        streams_[fret].open(pcm);
        streams_[fret].controls(
            1, 0, float(BASS_GetConfig(BASS_CONFIG_BUFFER)) / 1000);
    }
}
void SustainedFailPreview::press(unsigned fret, float volume) {
    if (fret >= 5 || !std::isfinite(volume) || volume < 0 || volume > 1) {
        throw Error("Invalid sustained fret/volume");
    }
    // SetAttribute did not cancel slides in older BASS revisions.
    check(BASS_ChannelSlideAttribute(streams_[fret].handle(), BASS_ATTRIB_VOL,
                                     volume, 0),
          "Reset sustained fade");
    streams_[fret].position(0);
    streams_[fret].play();
}
void SustainedFailPreview::release(unsigned fret, unsigned milliseconds) {
    if (fret >= 5 || milliseconds > 10000) {
        throw Error("Invalid sustained release");
    }
    check(BASS_ChannelSlideAttribute(streams_[fret].handle(), BASS_ATTRIB_VOL,
                                     -1, milliseconds),
          "Fade sustained fret");
}
DWORD SustainedFailPreview::handle(unsigned fret) const {
    if (fret >= 5) {
        throw Error("Invalid sustained fret");
    }
    return streams_[fret].handle();
}
void SustainedFailPreview::stop() {
    for (auto &stream : streams_) {
        if (stream.handle()) {
            check(BASS_ChannelSlideAttribute(stream.handle(), BASS_ATTRIB_VOL,
                                             0, 0),
                  "Cancel sustained fade");
            stream.stop();
        }
    }
}
} // namespace gpp::audio
