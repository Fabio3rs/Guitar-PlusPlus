#include "voice_benchmark.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <unistd.h>

namespace AudioStudio {
namespace {
using Clock = std::chrono::steady_clock;
using Alloc = Json::AllocatorType;
struct Memory {
    int64_t rss = -1, pss = -1, peak = -1;
};
Memory memory() {
    Memory result;
    for (const char *path : {"/proc/self/status", "/proc/self/smaps_rollup"}) {
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream row(line);
            std::string key;
            int64_t kb = 0;
            if (row >> key >> kb) {
                if (key == "VmRSS:" || key == "Rss:") {
                    result.rss = kb * 1024;
                }
                if (key == "Pss:") {
                    result.pss = kb * 1024;
                }
                if (key == "VmHWM:") {
                    result.peak = kb * 1024;
                }
            }
        }
    }
    return result;
}

Value memoryJson(const Memory &m, Alloc &a) {
    Value v(rapidjson::kObjectType);
    v.AddMember("rss_bytes", m.rss < 0 ? Value() : Value(m.rss), a);
    v.AddMember("pss_bytes", m.pss < 0 ? Value() : Value(m.pss), a);
    v.AddMember("peak_rss_bytes", m.peak < 0 ? Value() : Value(m.peak), a);
    return v;
}

struct Sample {
    std::unique_ptr<audio::PreparedSample> sample;
    HSAMPLE handle = 0;
    void clear() {
        sample.reset();
        handle = 0;
    }
};

struct Event {
    double acquire = 0, restart = 0, play = 0, at = 0, scheduled = 0;
    unsigned active = 0;
    int error = 0;
    bool accepted = false, replaced = false;
};

template <class F> double timed(F action) {
    const auto start = Clock::now();
    action();
    return std::chrono::duration<double, std::milli>(Clock::now() - start)
        .count();
}

double percentile(std::vector<double> values, double p) {
    if (values.empty()) {
        return 0;
    }
    std::sort(values.begin(), values.end());
    return values[size_t(std::ceil(p * values.size())) - 1];
}
} // namespace

int voiceProbe(const fs::path &configPath, const fs::path &pcmPath,
               const fs::path &reportPath) {
    std::ifstream configFile(configPath);
    const std::string text((std::istreambuf_iterator<char>(configFile)), {});
    auto config = parseJson(text);
    // Config is also a CLI boundary: reject incomplete or incorrectly typed
    // input.
    if (!config.IsObject()) {
        throw audio::Error("Invalid probe config");
    }
    for (const char *key :
         {"voices", "bursts", "interval_ms", "device", "rate", "buffer_ms",
          "update_ms", "device_buffer_ms", "device_period_ms"}) {
        if (!config.HasMember(key) || !config[key].IsInt()) {
            throw audio::Error("Invalid probe integer field");
        }
    }
    if (!config.HasMember("strategy") || !config["strategy"].IsString() ||
        !config.HasMember("steal_oldest") || !config["steal_oldest"].IsBool() ||
        !config.HasMember("stream_buffer") ||
        !config["stream_buffer"].IsNumber()) {
        throw audio::Error("Invalid probe config fields");
    }
    const std::string strategy = config["strategy"].GetString();
    if (strategy != "hsample" && strategy != "streams") {
        throw audio::Error("Unknown strategy");
    }
    VoiceBenchmarkOptions o;
    o.voices = config["voices"].GetInt();
    o.bursts = config["bursts"].GetInt();
    o.intervalMs = config["interval_ms"].GetInt();
    o.stealOldest = config["steal_oldest"].GetBool();
    validate(o);
    audio::DeviceSettings d;
    d.device = config["device"].GetInt();
    d.rate = config["rate"].GetInt();
    d.bufferMs = config["buffer_ms"].GetInt();
    d.updateMs = config["update_ms"].GetInt();
    d.deviceBufferMs = config["device_buffer_ms"].GetInt();
    d.devicePeriodMs = config["device_period_ms"].GetInt();
    const double buffer = config["stream_buffer"].GetDouble();
    if (!std::isfinite(buffer) || buffer < 0 || buffer > 5) {
        throw audio::Error("Invalid stream buffer");
    }
    audio::BassSystem bass(d);
    // Preallocate instrumentation before the baseline so event storage is not
    // misreported as incremental audio memory.
    std::vector<Event> events(size_t(o.bursts + 1) * o.voices * 2);
    std::vector<DWORD> channels(size_t(o.voices), 0), active;
    active.reserve(o.voices);
    std::vector<uint64_t> sequence(o.voices, 0);
    const Memory baseline = memory();
    auto decoder = audio::openFile(pcmPath, true);
    const auto info = audio::sourceInfo(decoder.get());
    decoder.reset();
    if (info.duration > 2 || info.duration <= 0) {
        throw audio::Error("PCM duration must be within 2 seconds");
    }
    auto pcm = audio::readSegment(pcmPath, 0, info.duration);
    const uint64_t pcmBytes = pcm.samples.size() * sizeof(float);
    uint64_t pcmHash = 14695981039346656037ull;
    const auto *pcmData =
        reinterpret_cast<const unsigned char *>(pcm.samples.data());
    for (size_t i = 0; i < pcmBytes; ++i) {
        pcmHash = (pcmHash ^ pcmData[i]) * 1099511628211ull;
    }
    const uint64_t pcmCapacity = pcm.samples.capacity() * sizeof(float);
    const Memory loaded = memory();
    std::vector<uint8_t> wav;
    std::vector<audio::Stream> streams;
    Sample sample;
    const double prepare = timed([&] {
        if (strategy == "hsample") {
            sample.sample = std::make_unique<audio::PreparedSample>(
                pcm, unsigned(o.voices), o.stealOldest);
            sample.handle = sample.sample->handle();
            BASS_SAMPLE defaults{};
            audio::check(BASS_SampleGetInfo(sample.handle, &defaults),
                         "Read sample defaults");
            defaults.volume = 0;
            defaults.mingap = 0;
            audio::check(BASS_SampleSetInfo(sample.handle, &defaults),
                         "Set muted sample defaults");
        } else {
            wav = audio::wavBytes(pcm, true);
            streams.reserve(o.voices);
            for (int i = 0; i < o.voices; ++i) {
                streams.emplace_back(BASS_StreamCreateFile(
                    TRUE, wav.data(), 0, wav.size(), BASS_SAMPLE_FLOAT));
                audio::check(streams.back().get() != 0,
                             "Create shared-memory stream");
                channels[i] = streams.back().get();
                audio::check(
                    BASS_ChannelSetAttribute(channels[i], BASS_ATTRIB_VOL, 0),
                    "Mute stream");
                audio::check(BASS_ChannelSetAttribute(channels[i],
                                                      BASS_ATTRIB_BUFFER,
                                                      float(buffer)),
                             "Set stream buffer");
            }
        }
    });
    const auto wavCapacity = wav.capacity();
    const Memory prepared = memory();
    Memory playing = prepared;
    int maxActive = 0;
    uint64_t serial = 0;
    auto collectActive = [&] {
        active.clear();
        if (sample.handle) {
            const DWORD count =
                BASS_SampleGetChannels(sample.handle, channels.data());
            audio::check(count != DWORD(-1) && count <= DWORD(o.voices),
                         "List sample voices");
            for (DWORD i = 0; i < count; ++i) {
                if (BASS_ChannelIsActive(channels[i]) == BASS_ACTIVE_PLAYING) {
                    active.push_back(channels[i]);
                }
            }
        } else {
            for (DWORD ch : channels) {
                if (BASS_ChannelIsActive(ch) == BASS_ACTIVE_PLAYING) {
                    active.push_back(ch);
                }
            }
        }
    };
    const auto start = Clock::now();
    for (int burst = 0; burst <= o.bursts; ++burst) {
        const auto burstStart = Clock::now();
        for (int index = 0; index < o.voices * 2; ++index) {
            std::this_thread::sleep_until(
                burstStart + std::chrono::milliseconds(index * o.intervalMs));
            auto &event = events[size_t(burst) * o.voices * 2 + index];
            collectActive();
            event.at =
                std::chrono::duration<double, std::milli>(Clock::now() - start)
                    .count();
            event.scheduled =
                std::chrono::duration<double, std::milli>(burstStart - start)
                    .count() +
                index * o.intervalMs;
            DWORD channel = 0;
            int slot = -1;
            event.acquire = timed([&] {
                if (sample.handle) {
                    channel = BASS_SampleGetChannel(sample.handle, 0);
                    if (!channel) {
                        event.error = BASS_ErrorGetCode();
                    }
                } else {
                    for (int i = 0; i < o.voices; ++i) {
                        if (std::find(active.begin(), active.end(),
                                      channels[i]) == active.end()) {
                            slot = i;
                            break;
                        }
                    }
                    if (slot < 0 && o.stealOldest) {
                        slot = int(
                            std::min_element(sequence.begin(), sequence.end()) -
                            sequence.begin());
                    }
                    if (slot >= 0) {
                        channel = channels[slot];
                    } else {
                        event.error = BASS_ERROR_NOCHAN;
                    }
                }
            });
            if (channel) {
                event.replaced = active.size() == size_t(o.voices);
                bool ok = false;
                event.restart = timed([&] {
                    ok = BASS_ChannelSetPosition(channel, 0, BASS_POS_BYTE) !=
                         FALSE;
                });
                if (!ok) {
                    event.error = BASS_ErrorGetCode();
                } else {
                    event.play = timed([&] {
                        ok = BASS_ChannelPlay(channel, FALSE) != FALSE;
                    });
                    if (!ok) {
                        event.error = BASS_ErrorGetCode();
                    }
                }
                event.accepted = ok;
                if (ok && slot >= 0) {
                    sequence[slot] = ++serial;
                }
            }
            // Observe API state only; this does not prove audible overlap.
            collectActive();
            event.active = unsigned(active.size());
            maxActive = std::max(maxActive, int(event.active));
        }
        const auto snapshot = memory();
        if (snapshot.rss > playing.rss) {
            playing = snapshot;
        }
        // Drain by observed state as device buffering can outlive nominal PCM
        // duration. Do not let the previous burst occupy the next burst's pool.
        const auto deadline =
            Clock::now() + std::chrono::duration<double>(pcm.seconds() + 5.);
        for (;;) {
            collectActive();
            if (active.empty()) {
                break;
            }
            if (Clock::now() >= deadline) {
                throw audio::Error(
                    "Voices did not finish within playback grace period");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    streams.clear();
    sample.clear();
    std::vector<uint8_t>().swap(wav);
    const Memory released =
        memory(); // Retain input PCM, then release it separately.
    pcm = {};
    const Memory pcmReleased = memory();
    Json report(rapidjson::kObjectType);
    auto &a = report.GetAllocator();
    report.AddMember("strategy", Value(strategy.c_str(), a), a);
    report.AddMember("voices", o.voices, a);
    report.AddMember("pid", int(getpid()), a);
    report.AddMember("device", BASS_GetDevice(), a);
    report.AddMember("no_sound", BASS_GetDevice() == 0, a);
    BASS_INFO deviceInfo{};
    audio::check(BASS_GetInfo(&deviceInfo), "Read actual device info");
    report.AddMember("actual_rate", deviceInfo.freq, a);
    report.AddMember("actual_init_flags", deviceInfo.initflags, a);
    report.AddMember(
        "bass_reported_latency_ms",
        BASS_GetDevice() == 0 ? Value() : Value(deviceInfo.latency), a);
    report.AddMember("bass_version", BASS_GetVersion(), a);
    report.AddMember("build_type", GPP_AUDIO_BUILD_TYPE, a);
    report.AddMember("compiler", GPP_AUDIO_COMPILER, a);
    report.AddMember("configuration", Value(config, a), a);
    report.AddMember("prepare_ms", prepare, a);
    report.AddMember("pcm_bytes", pcmBytes, a);
    report.AddMember("pcm_fnv1a64", Value(std::to_string(pcmHash).c_str(), a),
                     a);
    report.AddMember("pcm_capacity_bytes", pcmCapacity, a);
    report.AddMember("shared_wav_capacity_bytes", uint64_t(wavCapacity), a);
    report.AddMember(
        "memory_note",
        "RSS/PSS are process observations, not exact BASS allocations. Null "
        "means proc metric unavailable; HWM includes prior stages. Allocators "
        "may retain released pages. Active snapshot is taken after each burst, "
        "not a guaranteed transient peak. Input PCM is retained until "
        "pcm_released.",
        a);
    Value snapshots(rapidjson::kObjectType);
    snapshots.AddMember("bass_initialized", memoryJson(baseline, a), a);
    snapshots.AddMember("pcm_loaded", memoryJson(loaded, a), a);
    snapshots.AddMember("prepared", memoryJson(prepared, a), a);
    snapshots.AddMember("after_bursts", memoryJson(playing, a), a);
    snapshots.AddMember("strategy_released", memoryJson(released, a), a);
    snapshots.AddMember("pcm_released", memoryJson(pcmReleased, a), a);
    report.AddMember("memory", snapshots, a);
    Value raw(rapidjson::kArrayType), warmup(rapidjson::kArrayType);
    std::vector<double> acquire, restart, play;
    int rejected = 0, replaced = 0, failed = 0;
    for (size_t i = 0; i < events.size(); ++i) {
        const auto &e = events[i];
        Value event(rapidjson::kObjectType);
        event.AddMember("at_ms", e.at, a);
        event.AddMember("scheduled_ms", e.scheduled, a);
        event.AddMember("scheduling_lateness_ms", e.at - e.scheduled, a);
        event.AddMember("acquire_ms", e.acquire, a);
        event.AddMember("restart_ms", e.restart, a);
        event.AddMember("play_ms", e.accepted ? Value(e.play) : Value(), a);
        event.AddMember("active", e.active, a);
        event.AddMember("accepted", e.accepted, a);
        event.AddMember("replaced", e.replaced, a);
        event.AddMember("error", e.error, a);
        if (i < size_t(o.voices * 2)) {
            warmup.PushBack(event, a);
        } else {
            acquire.push_back(e.acquire);
            if (e.accepted) {
                play.push_back(e.play);
                restart.push_back(e.restart);
            }
            rejected += e.error == BASS_ERROR_NOCHAN;
            failed += !e.accepted && e.error != BASS_ERROR_NOCHAN;
            replaced += e.accepted && e.replaced;
            raw.PushBack(event, a);
        }
    }
    Value summary(rapidjson::kObjectType);
    summary.AddMember("acquire_p95_ms", percentile(acquire, .95), a);
    summary.AddMember("play_p50_ms",
                      play.empty() ? Value() : Value(percentile(play, .5)), a);
    summary.AddMember("play_p95_ms",
                      play.empty() ? Value() : Value(percentile(play, .95)), a);
    summary.AddMember("rejected", rejected, a);
    summary.AddMember("failed", failed, a);
    summary.AddMember("replaced", replaced, a);
    summary.AddMember("max_active", maxActive, a);
    summary.AddMember("rss_growth_bytes",
                      baseline.rss < 0 || playing.rss < 0
                          ? Value()
                          : Value(playing.rss - baseline.rss),
                      a);
    summary.AddMember("rss_strategy_growth_bytes",
                      loaded.rss < 0 || playing.rss < 0
                          ? Value()
                          : Value(playing.rss - loaded.rss),
                      a);
    summary.AddMember("pss_strategy_growth_bytes",
                      loaded.pss < 0 || playing.pss < 0
                          ? Value()
                          : Value(playing.pss - loaded.pss),
                      a);
    report.AddMember("summary", summary, a);
    Value metrics(rapidjson::kObjectType);
    auto statistics = [&](const char *name, const std::vector<double> &values) {
        Value stats(rapidjson::kObjectType);
        stats.AddMember("count", uint64_t(values.size()), a);
        for (const auto &quantile : {std::make_pair("p50_ms", .5),
                                     {"p95_ms", .95},
                                     {"p99_ms", .99},
                                     {"max_ms", 1.}}) {
            stats.AddMember(Value(quantile.first, a),
                            values.empty()
                                ? Value()
                                : Value(percentile(values, quantile.second)),
                            a);
        }
        stats.AddMember(
            "mean_ms",
            values.empty()
                ? Value()
                : Value(std::accumulate(values.begin(), values.end(), 0.) /
                        values.size()),
            a);
        metrics.AddMember(Value(name, a), stats, a);
    };
    statistics("acquire", acquire);
    statistics("restart_successful", restart);
    statistics("play_successful", play);
    report.AddMember("metrics", metrics, a);
    report.AddMember("warmup_events", warmup, a);
    report.AddMember("events", raw, a);
    saveJson(reportPath, report);
    return 0;
}
int cacheProbe(const fs::path &configPath, const fs::path &bankPath,
               const fs::path &reportPath) {
    std::ifstream input(configPath);
    auto config =
        parseJson(std::string((std::istreambuf_iterator<char>(input)), {}));
    if (!config.IsObject() || !config.HasMember("source") ||
        !config["source"].IsString()) {
        throw audio::Error("Invalid cache probe source");
    }
    for (const char *name :
         {"device", "rate", "voices", "iterations", "buffer_ms", "update_ms",
          "device_buffer_ms", "device_period_ms"}) {
        if (!config.HasMember(name) || !config[name].IsInt()) {
            throw audio::Error("Invalid cache probe setting");
        }
    }
    if (!config.HasMember("steal_oldest") || !config["steal_oldest"].IsBool()) {
        throw audio::Error("Invalid voice policy");
    }
    audio::DeviceSettings d;
    d.device = config["device"].GetInt();
    d.rate = config["rate"].GetInt();
    d.bufferMs = config["buffer_ms"].GetInt();
    d.updateMs = config["update_ms"].GetInt();
    d.deviceBufferMs = config["device_buffer_ms"].GetInt();
    d.devicePeriodMs = config["device_period_ms"].GetInt();
    const int iterations = config["iterations"].GetInt(),
              voices = config["voices"].GetInt();
    if (iterations < 1 || iterations > 1000 || voices < 1 || voices > 256) {
        throw audio::Error("Invalid cache probe counts");
    }
    struct Trial {
        double ms = 0, time = 0;
        unsigned mask = 0;
        bool accepted = false;
    };
    std::vector<Trial> trials(size_t(iterations), Trial{});
    audio::BassSystem system(d);
    const auto baseline = memory();
    const auto loadStart = Clock::now();
    auto cache = std::make_shared<audio::SongFailCache>(
        audio::loadFailCache(bankPath, config["source"].GetString()));
    const double loadMs =
        std::chrono::duration<double, std::milli>(Clock::now() - loadStart)
            .count();
    const auto loaded = memory();
    const auto pcmBytes = cache->pcmBytes();
    auto bank = std::make_unique<audio::PreparedFailBank>(
        cache,
        audio::BankOptions{unsigned(voices), config["steal_oldest"].GetBool()});
    const auto prepared = memory();
    bank->trigger(0, 1, 0);
    const double firstMs = bank->stats().lastTriggerMs;
    bank->stop();
    bank->resetStatistics();
    for (int i = 0; i < iterations; ++i) {
        if (i % (2 * voices) == 0) {
            bank->stop();
        }
        auto &trial = trials[size_t(i)];
        trial.time = cache->song.info.duration * i / iterations;
        trial.mask = i % 3 == 0 && voices >= 5 ? 31u : 1u << (i % 5);
        trial.accepted = bank->trigger(trial.time, trial.mask, 0);
        trial.ms = bank->stats().lastTriggerMs;
    }
    const auto playing = memory();
    const auto stats = bank->stats();
    bank.reset();
    const auto released = memory();
    cache.reset();
    const auto freed = memory();
    Json report(rapidjson::kObjectType);
    auto &a = report.GetAllocator();
    report.AddMember("pid", int(getpid()), a);
    report.AddMember("device", BASS_GetDevice(), a);
    report.AddMember("bass_version", BASS_GetVersion(), a);
    report.AddMember("fx_version", BASS_FX_GetVersion(), a);
    report.AddMember("no_sound", BASS_GetDevice() == 0, a);
    report.AddMember("audible_latency_ms", Value(), a);
    report.AddMember("pcm_bytes", pcmBytes, a);
    report.AddMember("file_bytes", uint64_t(fs::file_size(bankPath)), a);
    report.AddMember("load_validate_ms", loadMs, a);
    report.AddMember("prepare_ms", stats.prepareMs, a);
    report.AddMember("first_trigger_ms", firstMs, a);
    report.AddMember("accepted", stats.accepted, a);
    report.AddMember("rejected", stats.rejected, a);
    report.AddMember("replaced", stats.replaced, a);
    report.AddMember("failures", stats.failures, a);
    report.AddMember("peak_active", stats.peakActive, a);
    Value memoryReport(rapidjson::kObjectType);
    memoryReport.AddMember("baseline", memoryJson(baseline, a), a);
    memoryReport.AddMember("loaded", memoryJson(loaded, a), a);
    memoryReport.AddMember("prepared", memoryJson(prepared, a), a);
    memoryReport.AddMember("playing", memoryJson(playing, a), a);
    memoryReport.AddMember("bank_released", memoryJson(released, a), a);
    memoryReport.AddMember("pcm_released", memoryJson(freed, a), a);
    report.AddMember("memory", memoryReport, a);
    Value events(rapidjson::kArrayType);
    for (const auto &trial : trials) {
        Value event(rapidjson::kObjectType);
        event.AddMember("song_seconds", trial.time, a);
        event.AddMember("mask", trial.mask, a);
        event.AddMember("accepted", trial.accepted, a);
        event.AddMember("trigger_ms", trial.ms, a);
        events.PushBack(event, a);
    }
    report.AddMember("events", events, a);
    report.AddMember(
        "method",
        "Fresh exec; all PCM loaded and uploaded; muted sequential bursts of "
        "up to 2*voice-limit requests, stopped between bursts. One separate "
        "warmup. Memory snapshots outside API timers; allocator/page effects "
        "are included.",
        a);
    saveJson(reportPath, report);
    return 0;
}
} // namespace AudioStudio

int main(int argc, char **argv) {
    try {
        if (argc == 5 && std::string(argv[1]) == "--cache") {
            return AudioStudio::cacheProbe(argv[2], argv[3], argv[4]);
        }
        if (argc != 4) {
            throw gpp::audio::Error(
                "Usage: gpp_audio_voice_probe CONFIG PCM REPORT");
        }
        return AudioStudio::voiceProbe(argv[1], argv[2], argv[3]);
    } catch (const std::exception &error) {
        std::cerr << "Voice probe: " << error.what() << '\n';
        return 1;
    }
}
