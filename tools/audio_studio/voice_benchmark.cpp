#include "voice_benchmark.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <set>
#ifdef __linux__
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace AudioStudio {
void validate(const VoiceBenchmarkOptions &o) {
    if (o.voices < 1 || o.voices > 32 || o.bursts < 1 || o.bursts > 100 ||
        o.intervalMs < 0 || o.intervalMs > 100) {
        throw audio::Error(
            "Voice benchmark: voices 1..32, bursts 1..100, interval 0..100 ms");
    }
}

Json loadVoiceReport(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw audio::Error("Cannot open voice report");
    }
    std::string data(32 * 1024 * 1024 + 1, '\0');
    in.read(data.data(), std::streamsize(data.size()));
    data.resize(size_t(in.gcount()));
    if (data.size() > 32 * 1024 * 1024) {
        throw audio::Error("Voice report exceeds 32 MiB");
    }
    Json result;
    result.Parse(data.c_str(), data.size());
    if (result.HasParseError() || !result.IsObject() ||
        !result.HasMember("kind") || !result["kind"].IsString() ||
        std::string(result["kind"].GetString()) != "voice_comparison" ||
        !result.HasMember("schema_version") ||
        !result["schema_version"].IsInt() ||
        result["schema_version"].GetInt() != 1 || !result.HasMember("cases") ||
        !result["cases"].IsArray()) {
        throw audio::Error("Expected a voice_comparison report");
    }
    // Validate every field consumed by the GUI before publishing imported data.
    for (const auto &c : result["cases"].GetArray()) {
        if (!c.IsObject() || !c.HasMember("strategy") ||
            !c["strategy"].IsString() || !c.HasMember("voices") ||
            !c["voices"].IsInt() || !c.HasMember("summary") ||
            !c["summary"].IsObject()) {
            throw audio::Error("Invalid voice comparison case");
        }
        for (const char *key : {"play_p95_ms", "acquire_p95_ms", "rejected",
                                "replaced", "max_active", "rss_growth_bytes"}) {
            if (!c["summary"].HasMember(key) ||
                !(c["summary"][key].IsNumber() || c["summary"][key].IsNull())) {
                throw audio::Error("Invalid voice comparison summary");
            }
        }
    }
    return result;
}

Json compareVoices(const audio::PcmBuffer &pcm,
                   const VoiceBenchmarkOptions &options,
                   audio::DeviceSettings device, float bufferSeconds,
                   const fs::path &outputDirectory,
                   const audio::Checkpoint &checkpoint) {
    validate(options);
#ifndef __linux__
    throw audio::Error(
        "Isolated voice/memory benchmarking currently requires Linux");
#else
    if (pcm.seconds() > 2 || pcm.samples.empty()) {
        throw audio::Error(
            "Voice benchmark requires a generated effect of at most 2 seconds");
    }
    const double secondsPerCase =
        (options.bursts + 1) *
        (2 * options.voices * options.intervalMs / 1000. + pcm.seconds() + .02);
    if (secondsPerCase > 150) {
        throw audio::Error("Reduce burst count/interval: maximum estimated "
                           "duration is 150 seconds per case");
    }
    fs::path scratch;
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    for (int i = 0; i < 100; ++i) {
        auto candidate =
            outputDirectory /
            ("voice-probe-" + std::to_string(stamp) + "-" + std::to_string(i));
        if (fs::create_directory(candidate)) {
            scratch = candidate;
            break;
        }
    }
    if (scratch.empty()) {
        throw audio::Error("Cannot reserve benchmark directory");
    }
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code error;
            fs::remove_all(path, error);
        }
    } cleanup{scratch};
    auto input = pcm;
    input.loop = {};
    audio::writeWav(scratch / "pcm.wav", input, true);
    const auto executable = fs::read_symlink("/proc/self/exe").parent_path() /
                            "gpp_audio_voice_probe";
    std::set<int> sizes;
    for (int count : {1, 5, 10, 20, 32}) {
        if (count <= options.voices) {
            sizes.insert(count);
        }
    }
    sizes.insert(options.voices);
    Json report(rapidjson::kObjectType);
    auto &a = report.GetAllocator();
    report.AddMember("kind", "voice_comparison", a);
    report.AddMember("schema_version", 1, a);
    report.AddMember(
        "created_unix_seconds",
        int64_t(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count()),
        a);
    report.AddMember("pcm_seconds", input.seconds(), a);
    report.AddMember("pcm_bytes",
                     uint64_t(input.samples.size() * sizeof(float)), a);
    report.AddMember("trim_tail", options.trimTail, a);
    report.AddMember("burst_count", options.bursts, a);
    report.AddMember("interval_ms", options.intervalMs, a);
    report.AddMember("steal_oldest", options.stealOldest, a);
    report.AddMember(
        "method",
        "Each strategy/voice count uses a fresh exec process, identical float "
        "PCM, muted channels, one warmup burst then measured bursts. Each "
        "burst attempts twice the voice limit. Streams share one memory WAV. "
        "Sample voices are acquired on demand; streams are created upfront. "
        "Memory snapshots and state polling are outside API timing. NoSound "
        "and muted runs cannot validate audible overlap or glitches.",
        a);
    report.AddMember("pending",
                     "Final-mix marker, timestamped system loopback, physical "
                     "latency, audible glitch detection and continuous gates.",
                     a);
    report.AddMember("audible_latency_ms", Value(), a);
    Value cases(rapidjson::kArrayType);
    size_t completed = 0;
    for (int voices : sizes) {
        for (const char *strategy : {"hsample", "streams"}) {
            checkpoint(double(completed) / (2 * sizes.size()));
            Json config(rapidjson::kObjectType);
            auto &ca = config.GetAllocator();
            config.AddMember("strategy", Value(strategy, ca), ca);
            config.AddMember("voices", voices, ca);
            config.AddMember("bursts", options.bursts, ca);
            config.AddMember("interval_ms", options.intervalMs, ca);
            config.AddMember("steal_oldest", options.stealOldest, ca);
            config.AddMember("device", device.device, ca);
            config.AddMember("rate", device.rate, ca);
            config.AddMember("buffer_ms", device.bufferMs, ca);
            config.AddMember("update_ms", device.updateMs, ca);
            config.AddMember("device_buffer_ms", device.deviceBufferMs, ca);
            config.AddMember("device_period_ms", device.devicePeriodMs, ca);
            config.AddMember("stream_buffer", bufferSeconds, ca);
            saveJson(scratch / "config.json", config);
            std::vector<std::string> args{executable.string(),
                                          (scratch / "config.json").string(),
                                          (scratch / "pcm.wav").string(),
                                          (scratch / "result.json").string()};
            std::vector<char *> argv;
            for (auto &arg : args) {
                argv.push_back(arg.data());
            }
            argv.push_back(nullptr);
            pid_t pid = 0;
            const int error = posix_spawn(&pid, executable.c_str(), nullptr,
                                          nullptr, argv.data(), environ);
            if (error) {
                throw audio::Error("Cannot start voice probe: " +
                                   std::to_string(error));
            }
            int status = 0;
            try {
                const auto start = std::chrono::steady_clock::now();
                for (;;) {
                    const auto result = waitpid(pid, &status, WNOHANG);
                    if (result == pid) {
                        break;
                    }
                    if (result < 0 && errno != EINTR) {
                        if (errno == ECHILD) {
                            pid = -1;
                        }
                        throw audio::Error("Cannot wait for voice probe");
                    }
                    checkpoint(double(completed) / (2 * sizes.size()));
                    if (std::chrono::steady_clock::now() - start >
                        std::chrono::seconds(180)) {
                        throw audio::Error(
                            "Voice probe timed out (180 seconds per case)");
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            } catch (...) {
                if (pid > 0) {
                    kill(pid, SIGKILL);
                    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
                    }
                }
                throw;
            }
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                throw audio::Error("Voice probe failed; see stderr. Requested "
                                   "device is not replaced by NoSound.");
            }
            std::ifstream file(scratch / "result.json");
            std::string content((std::istreambuf_iterator<char>(file)), {});
            Json child;
            child.Parse(content.c_str(), content.size());
            if (child.HasParseError() || !child.IsObject()) {
                throw audio::Error("Invalid voice probe result");
            }
            cases.PushBack(Value(child, a), a);
            ++completed;
        }
    }
    for (auto &c : cases.GetArray()) {
        auto &summary = c["summary"];
        Value marginal;
        for (const auto &base : cases.GetArray()) {
            if (base["voices"].GetInt() == 1 &&
                std::string(base["strategy"].GetString()) ==
                    c["strategy"].GetString() &&
                c["voices"].GetInt() > 1 &&
                summary["rss_strategy_growth_bytes"].IsNumber() &&
                base["summary"]["rss_strategy_growth_bytes"].IsNumber()) {
                marginal.SetDouble(
                    (summary["rss_strategy_growth_bytes"].GetDouble() -
                     base["summary"]["rss_strategy_growth_bytes"].GetDouble()) /
                    (c["voices"].GetInt() - 1));
            }
        }
        summary.AddMember("rss_bytes_per_extra_voice_vs_one", marginal, a);
    }
    report.AddMember(
        "marginal_memory_note",
        "Per-extra-voice RSS is a difference between independent processes "
        "relative to the one-voice case, not an exact allocation size. Page "
        "granularity and allocator noise can produce negative differences.",
        a);
    report.AddMember("cases", cases, a);
    checkpoint(1);
    return report;
#endif
}
} // namespace AudioStudio
