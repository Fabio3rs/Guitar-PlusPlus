#include "session.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <numeric>

namespace AudioStudio {
namespace {
using Clock = std::chrono::steady_clock;
using Alloc = Json::AllocatorType;
void string(Value &object, const char *key, const std::string &text, Alloc &a) {
    object.AddMember(Value(key, a), Value(text.c_str(), a), a);
}
struct Metric {
    double first = 0;
    std::vector<double> samples;
};
}

Json runBenchmark(const fs::path &source, double time,
                  const audio::Settings &settings, int iterations,
                  float bufferSeconds, const audio::Checkpoint &progress) {
    if (iterations < 1 || iterations > 1000) {
        throw audio::Error("Benchmark iterations must be between 1 and 1000");
    }
    std::map<std::string, Metric> metrics;
    audio::AudioTransport reused;
    size_t pcmBytes = 0;
    double pcmSeconds = 0;
    const auto started = Clock::now();
    for (int run = 0; run <= iterations; ++run) {
        progress(double(run) / (iterations + 1));
        auto checkpoint = [&](double) { progress(double(run) / (iterations + 1)); };
        auto record = [&](const std::string &name, double ms) {
            auto &metric = metrics[name];
            if (run == 0) {
                metric.first = ms;
            } else {
                metric.samples.push_back(ms);
            }
        };
        auto timed = [&](const std::string &name, auto action) {
            const auto start = Clock::now();
            action();
            const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            record(name, ms);
        };
        audio::AnalysisResult analysis;
        audio::PcmBuffer raw, processed;
        timed("analysis", [&] { analysis = audio::analyze(source, time, settings.analysis, checkpoint); });
        timed("synthesis", [&] { raw = audio::synthesize(analysis, settings.synth, checkpoint); });
        timed("offline_fx", [&] { processed = audio::processFx(raw, settings.fx, checkpoint); });
        timed("loop_bake", [&] {
            audio::bakeLoop(raw, settings.synth.crossfade);
            audio::bakeLoop(processed, settings.synth.crossfade);
        });
        pcmBytes = processed.samples.size() * sizeof(float);
        pcmSeconds = processed.seconds();
        audio::AudioTransport file, pcm, live;
        timed("source_open", [&] { file.open(source); });
        file.controls(0, 0, bufferSeconds);
        timed("source_seek_api", [&] { file.position(time); });
        timed("source_prefill_api", [&] {
            audio::check(BASS_ChannelUpdate(file.handle(), 0), "Prefill source buffer");
        });
        timed("source_play_api", [&] { file.play(); });
        timed("source_stop_api", [&] { file.stop(); });
        timed("pcm_prepare", [&] { pcm.open(processed); });
        pcm.controls(0, 0, bufferSeconds);
        timed("pcm_prefill_api", [&] {
            audio::check(BASS_ChannelUpdate(pcm.handle(), 0), "Prefill PCM buffer");
        });
        timed("pcm_play_api", [&] { pcm.play(); });
        timed("pcm_stop_api", [&] { pcm.stop(); });
        timed("live_pcm_prepare", [&] { live.open(raw); });
        live.controls(0, 0, bufferSeconds);
        timed("live_fx_attach", [&] { live.effects(settings.fx); });
        timed("live_prefill_api", [&] {
            audio::check(BASS_ChannelUpdate(live.handle(), 0), "Prefill live FX buffer");
        });
        timed("live_play_api", [&] { live.play(); });
        live.stop();
        if (run == 0) {
            reused.open(processed);
            reused.controls(0, 0, bufferSeconds);
        }
        timed("reused_pcm_seek_api", [&] { reused.position(0); });
        timed("reused_pcm_play_api", [&] { reused.play(); });
        reused.stop();
    }
    Json report(rapidjson::kObjectType);
    auto &a = report.GetAllocator();
    report.AddMember("schema_version", 1, a);
    report.AddMember("iterations", iterations, a);
    report.AddMember("warmup_iterations", 1, a);
    string(report, "clock", "std::chrono::steady_clock", a);
    report.AddMember("created_unix_seconds", int64_t(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()), a);
    string(report, "build_type", GPP_AUDIO_BUILD_TYPE, a);
    string(report, "compiler", GPP_AUDIO_COMPILER, a);
    string(report, "device_setup_scope", "One wall-clock sample around device setup/configuration, including any fallback; not a distribution of BASS_Init alone.", a);
    string(report, "source", source.string(), a);
    string(report, "method", "Muted, sequential trials; dedicated channels. First trial reported separately; not a cold filesystem/device test. Source playback and synthesized PCM contain different audio. PCM uses memory WAV streams, not HSAMPLE. API completion is not audible onset. Prefill uses ChannelUpdate(0), not a fixed frame count.", a);
    report.AddMember("source_time_seconds", time, a);
    report.AddMember("total_ms", std::chrono::duration<double, std::milli>(Clock::now() - started).count(), a);
    report.AddMember("pcm_bytes", uint64_t(pcmBytes), a);
    report.AddMember("pcm_seconds", pcmSeconds, a);
    report.AddMember("bass_version", BASS_GetVersion(), a);
    report.AddMember("fx_version", BASS_FX_GetVersion(), a);
    report.AddMember("device", BASS_GetDevice(), a);
    report.AddMember("no_sound", BASS_GetDevice() == 0, a);
    BASS_DEVICEINFO device{};
    if (BASS_GetDeviceInfo(BASS_GetDevice(), &device)) {
        string(report, "device_name", device.name ? device.name : "", a);
        string(report, "device_driver", device.driver ? device.driver : "", a);
    }
    BASS_INFO info{};
    audio::check(BASS_GetInfo(&info), "Benchmark device information");
    report.AddMember("device_rate", info.freq, a);
    report.AddMember("device_init_flags", info.initflags, a);
    report.AddMember("bass_reported_latency_ms", BASS_GetDevice() ? Value(info.latency) : Value(), a);
    report.AddMember("channel_buffer_seconds", bufferSeconds, a);
    report.AddMember("channel_capacity_ms", BASS_GetConfig(BASS_CONFIG_BUFFER), a);
    report.AddMember("update_period_ms", BASS_GetConfig(BASS_CONFIG_UPDATEPERIOD), a);
    report.AddMember("device_buffer_ms", BASS_GetConfig(BASS_CONFIG_DEV_BUFFER), a);
    report.AddMember("device_period_ms", BASS_GetConfig(BASS_CONFIG_DEV_PERIOD), a);
    auto config = settingsJson(settings);
    report.AddMember("settings", Value(config, a), a);
    Value results(rapidjson::kObjectType);
    for (auto &entry : metrics) {
        auto sorted = entry.second.samples;
        std::sort(sorted.begin(), sorted.end());
        auto percentile = [&](double p) {
            return sorted[size_t(std::ceil(p * sorted.size())) - 1];
        };
        Value metric(rapidjson::kObjectType), samples(rapidjson::kArrayType);
        string(metric, "status", "measured", a);
        metric.AddMember("first_ms", entry.second.first, a);
        metric.AddMember("count", iterations, a);
        metric.AddMember("mean_ms", std::accumulate(sorted.begin(), sorted.end(), 0.) / sorted.size(), a);
        metric.AddMember("min_ms", sorted.front(), a);
        metric.AddMember("p50_ms", percentile(.5), a);
        metric.AddMember("p95_ms", percentile(.95), a);
        metric.AddMember("p99_ms", percentile(.99), a);
        metric.AddMember("max_ms", sorted.back(), a);
        for (double sample : entry.second.samples) {
            samples.PushBack(sample, a);
        }
        metric.AddMember("samples_ms", samples, a);
        results.AddMember(Value(entry.first.c_str(), a), metric, a);
    }
    report.AddMember("metrics", results, a);
    // Keep the existing CLI summary keys for automation clients.
    report.AddMember("analysis_mean_ms", report["metrics"]["analysis"]["mean_ms"].GetDouble(), a);
    report.AddMember("synth_mean_ms", report["metrics"]["synthesis"]["mean_ms"].GetDouble(), a);
    report.AddMember("fx_mean_ms", report["metrics"]["offline_fx"]["mean_ms"].GetDouble(), a);
    Value pending(rapidjson::kObjectType);
    string(pending, "command_to_final_mix", "Pending: timestamped marker probe is not implemented; muted trials cannot measure signal onset.", a);
    string(pending, "command_to_audio_system", "Pending: Linux loopback capture with sample timestamps and clock alignment is not implemented.", a);
    string(pending, "command_to_physical_output", "Pending: calibrated cable/microphone capture and an accessible output device are required.", a);
    string(pending, "sample_voices_gate_and_underruns", "Pending: HSAMPLE, continuous-gate, overlap, sustained playback and glitch benchmarks.", a);
    report.AddMember("pending", pending, a);
    report.AddMember("audible_latency_ms", Value(), a);
    progress(1);
    return report;
}
} // namespace AudioStudio
