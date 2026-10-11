#pragma once

#include <array>
#include <atomic>
#include <bass.h>
#include <bass_fx.h>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace gpp::audio {
struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

void check(bool ok, const char *operation);

// A stream and every attached FX have a single owner. Memory backing a file
// stream must outlive this handle; AudioTransport keeps it alongside the
// handle.
class Stream {
    HSTREAM handle_ = 0;

  public:
    explicit Stream(HSTREAM handle = 0) : handle_(handle) {}
    ~Stream();
    Stream(Stream &&other) noexcept;
    Stream &operator=(Stream &&other) noexcept;
    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;
    HSTREAM get() const { return handle_; }
    void reset() noexcept;
};

struct AnalysisSettings {
    int fft = 8192, midiMin = 35, midiMax = 88, candidates = 6, harmonics = 8;
    double window = .2, hop = .09, cents = 35, harmonicFalloff = 1.15;
    double threshold = .18, pruneRatio = .72;
};

struct SynthSettings {
    int rate = 44100, fretMask = 4, voices = 5, harmonics = 10;
    bool loop = false, normalize = true;
    double duration = .85, attack = .004, decay = .055, fadeStart = .10;
    double sustain = .30, loopStart = .12, loopBody = .40, release = .10;
    double crossfade = .010, detune = 8, transient = .18, transientDecay = 120;
    double bend = .020, bendDecay = 18, saturation = 2.6, spread = .45;
    double harmonicFalloff = .85, rms = .30, peak = .95;
    std::array<int, 5> shifts{{-12, -5, 0, 5, 12}};
    std::array<double, 5> gains{{1, .9, 1, .85, .75}};
};

struct EqSettings {
    bool enabled = true;
    double frequency = 850, gain = 7.5, bandwidth = 1.1, q = 0;
};

struct FxSettings {
    bool enabled = true, distortion = true, normalize = true;
    EqSettings pre, post{true, 2400, 4, 1.4, 0};
    double drive = 3.4, dry = .05, wet = 1, feedback = .04, volume = .75;
    double preRoll = .040, tail = .080, fade = .005, rms = .22, peak = .95;
};

struct Settings {
    AnalysisSettings analysis;
    SynthSettings synth;
    FxSettings fx;
};

// One parameter catalogue for validation, presets, RPC and the numeric UI.
// Names and units are part of the Studio preset interface (schema version 1).
template <class Visitor> void visitSettings(Settings &s, Visitor visit) {
#define PARAM(group, member, low, high)                                        \
    visit(#group "." #member, s.group.member, double(low), double(high))
    PARAM(analysis, fft, 512, 32768);
    PARAM(analysis, window, .001, 30);
    PARAM(analysis, hop, .001, 5);
    PARAM(analysis, midiMin, 0, 127);
    PARAM(analysis, midiMax, 0, 127);
    PARAM(analysis, candidates, 1, 32);
    PARAM(analysis, harmonics, 1, 32);
    PARAM(analysis, cents, 1, 200);
    PARAM(analysis, harmonicFalloff, 0, 4);
    PARAM(analysis, threshold, 0, 1);
    PARAM(analysis, pruneRatio, 0, 1);
    PARAM(synth, rate, 8000, 192000);
    PARAM(synth, fretMask, 0, 31);
    PARAM(synth, voices, 1, 32);
    PARAM(synth, harmonics, 1, 64);
    PARAM(synth, loop, 0, 1);
    PARAM(synth, normalize, 0, 1);
    PARAM(synth, duration, .005, 30);
    PARAM(synth, attack, .0001, 5);
    PARAM(synth, decay, .0001, 5);
    PARAM(synth, fadeStart, 0, 30);
    PARAM(synth, sustain, 0, 1);
    PARAM(synth, loopStart, .005, 5);
    PARAM(synth, loopBody, .01, 30);
    PARAM(synth, release, .001, 10);
    PARAM(synth, crossfade, 0, 1);
    PARAM(synth, detune, 0, 100);
    PARAM(synth, transient, 0, 2);
    PARAM(synth, transientDecay, .1, 500);
    PARAM(synth, bend, 0, .5);
    PARAM(synth, bendDecay, .1, 100);
    PARAM(synth, saturation, .01, 10);
    PARAM(synth, spread, 0, 1);
    PARAM(synth, harmonicFalloff, 0, 4);
    PARAM(synth, rms, .001, 1);
    PARAM(synth, peak, .01, 1);
    PARAM(fx, enabled, 0, 1);
    PARAM(fx, distortion, 0, 1);
    PARAM(fx, normalize, 0, 1);
    PARAM(fx, drive, 0, 5);
    PARAM(fx, dry, -5, 5);
    PARAM(fx, wet, -5, 5);
    PARAM(fx, feedback, -1, 1);
    PARAM(fx, volume, 0, 2);
    PARAM(fx, preRoll, 0, 1);
    PARAM(fx, tail, 0, 5);
    PARAM(fx, fade, 0, 1);
    PARAM(fx, rms, .001, 1);
    PARAM(fx, peak, .01, 1);
#undef PARAM
    for (int i = 0; i < 5; ++i) {
        visit("synth.shift" + std::to_string(i), s.synth.shifts[i], -48., 48.);
        visit("synth.gain" + std::to_string(i), s.synth.gains[i], 0., 2.);
    }
    for (int i = 0; i < 2; ++i) {
        auto &eq = i == 0 ? s.fx.pre : s.fx.post;
        std::string prefix = i == 0 ? "fx.pre." : "fx.post.";
        visit(prefix + "enabled", eq.enabled, 0., 1.);
        visit(prefix + "frequency", eq.frequency, 1., 96000.);
        visit(prefix + "gain", eq.gain, -15., 15.);
        visit(prefix + "bandwidth", eq.bandwidth, 0., 9.99);
        visit(prefix + "q", eq.q, 0., 1.);
    }
}

void validate(const Settings &settings);

struct LoopRegion {
    size_t begin = 0, end = 0;
}; // end exclusive
struct PcmBuffer {
    unsigned rate = 44100, channels = 2;
    std::vector<float> samples;
    LoopRegion loop;
    size_t frames() const { return channels ? samples.size() / channels : 0; }
    double seconds() const { return rate ? double(frames()) / rate : 0; }
};

struct Levels {
    double peak = 0, rms = 0;
    size_t clipped = 0;
};
Levels levels(const PcmBuffer &pcm);
void normalize(PcmBuffer &pcm, double rms, double peak);
struct Candidate {
    int midi = 0;
    double frequency = 0, score = 0;
    std::string name;
};
struct AnalysisResult {
    unsigned rate = 0;
    std::vector<float> spectrum;
    std::vector<Candidate> notes;
    double milliseconds = 0;
};
struct SourceInfo {
    unsigned rate = 0, channels = 0;
    double duration = 0;
};
struct Waveform {
    SourceInfo info;
    std::vector<float> low, high;
};
// Called between bounded chunks; throw Error to cancel without publishing.
using Checkpoint = std::function<void(double)>;
Stream openFile(const std::filesystem::path &path, bool decode);
SourceInfo sourceInfo(HSTREAM stream);
void seek(HSTREAM stream, double seconds);
Waveform waveform(const std::filesystem::path &path,
                  const Checkpoint &checkpoint = {});
AnalysisResult analyze(const std::filesystem::path &path, double time,
                       const AnalysisSettings &settings,
                       const Checkpoint &checkpoint = {});
PcmBuffer readSegment(const std::filesystem::path &path, double start,
                      double length, const Checkpoint &checkpoint = {});
PcmBuffer synthesize(const AnalysisResult &analysis,
                     const SynthSettings &settings,
                     const Checkpoint &checkpoint = {});
void bakeLoop(PcmBuffer &pcm, double crossfade);
PcmBuffer processFx(const PcmBuffer &raw, const FxSettings &settings,
                    const Checkpoint &checkpoint = {});
std::vector<uint8_t> wavBytes(const PcmBuffer &pcm, bool float32);
void writeWav(const std::filesystem::path &path, const PcmBuffer &pcm,
              bool float32);

class FxChain {
    DWORD channel_ = 0;
    std::array<HFX, 3> effects_{};

  public:
    ~FxChain();
    FxChain() = default;
    FxChain(FxChain &&other) noexcept;
    FxChain &operator=(FxChain &&other) noexcept;
    FxChain(const FxChain &) = delete;
    FxChain &operator=(const FxChain &) = delete;
    void clear() noexcept;
    void apply(DWORD channel, unsigned rate, const FxSettings &settings);
};

struct DeviceSettings {
    int device = -1, rate = 44100;
    int bufferMs = 500, updateMs = 100, deviceBufferMs = 40,
        devicePeriodMs = 10;
};
struct DeviceInfo {
    int id = 0;
    std::string name;
    bool enabled = false, isDefault = false;
};
std::vector<DeviceInfo> devices();
void validate(const DeviceSettings &settings);
class BassSystem {
    int device_ = -1;

  public:
    explicit BassSystem(const DeviceSettings &settings);
    ~BassSystem();
    BassSystem(const BassSystem &) = delete;
    BassSystem &operator=(const BassSystem &) = delete;
    int device() const { return device_; }
};

// Control-thread owned. BASS manages playback workers; no user PCM callback.
class AudioTransport {
    std::vector<uint8_t> memory_;
    Stream stream_;
    FxChain fx_;
    SourceInfo info_;
    struct LoopControl {
        std::atomic<QWORD> begin{0};
        std::atomic<int> error{0};
    } loopControl_;
    HSYNC loopSync_ = 0;
    static void CALLBACK loopCallback(HSYNC, DWORD channel, DWORD, void *user);

  public:
    ~AudioTransport() { close(); }
    int loopError() const { return loopControl_.error.load(); }
    double lastSeekMs = 0, lastPlayMs = 0;
    void close();
    void open(const std::filesystem::path &path,
              const FxSettings *fx = nullptr);
    void open(const PcmBuffer &pcm);
    void play();
    void pause();
    void stop();
    void position(double seconds);
    double position() const;
    void loop(double begin, double end, bool enabled);
    void controls(float volume, float pan, float bufferSeconds);
    void effects(const FxSettings &settings);
    DWORD handle() const { return stream_.get(); }
    const SourceInfo &info() const { return info_; }
};
} // namespace gpp::audio
