#pragma once
#include "audio.h"

namespace gpp::audio::sfx {
inline constexpr unsigned generatorVersion = 1;
inline constexpr size_t maxLayers = 32;
inline constexpr size_t maxPcmBytes = 64 * 1024 * 1024;

enum class Generator { Oscillator, Noise, Pluck };
enum class Wave { Sine, Triangle, Sawtooth, Square, Pulse };
enum class Color { White, Pink, Brown };
enum class FilterType { None, Lowpass, Highpass, Bandpass };
enum class Curve { Linear, Exponential };
const char *name(Generator value);
const char *name(Wave value);
const char *name(Color value);
const char *name(FilterType value);
const char *name(Curve value);

struct Envelope {
    double attack = .001, decay = .08, sustain = 0, release = .05;
    bool exponential = true;
};
struct Frequency {
    double startHz = 440, endHz = 440;
    Curve curve = Curve::Exponential;
};
struct Modulation {
    double rateHz = 5, pitchCents = 0, amplitude = 0;
};
struct Filter {
    FilterType type = FilterType::None;
    double cutoffHz = 2000, q = .707;
};
struct Layer {
    Generator type = Generator::Oscillator;
    bool enabled = true;
    double start = 0, duration = 0; // Zero duration means the remaining sound.
    double gain = .5, pan = 0, phase = 0, duty = .5, damping = .996;
    Wave waveform = Wave::Sine;
    Color color = Color::White;
    Frequency frequency;
    Envelope envelope;
    Modulation modulation;
    Filter filter;
};
struct Loop {
    bool enabled = false;
    double begin = 0, end = 0, crossfade = .02; // End zero means duration.
};
struct SoundDefinition {
    unsigned schemaVersion = 1;
    std::string id = "new_sfx", category = "interface", description;
    unsigned sampleRate = 44100, channels = 2;
    uint32_t seed = 12031;
    double duration = .3, outputGainDb = -6, peakLimit = .95;
    Loop loop;
    std::vector<Layer> layers{Layer{}};
};

// Pure offline DSP. No BASS initialization, graphics or device required.
void validate(const SoundDefinition &sound);
PcmBuffer render(const SoundDefinition &sound,
                 const Checkpoint &checkpoint = {});
// Text-only, isolated Lua; no standard libraries or gameplay registrations.
SoundDefinition parse(const std::string &text,
                      const std::string &source = "recipe");
SoundDefinition load(const std::filesystem::path &path);
std::string serialize(const SoundDefinition &sound);
// Existing targets require explicit replacement. Writes are staged then
// renamed.
void save(const std::filesystem::path &path, const SoundDefinition &sound,
          bool replace = false);
void exportWav(const std::filesystem::path &path, const PcmBuffer &pcm,
               bool float32, bool replace = false,
               const Checkpoint &checkpoint = {});
std::vector<std::filesystem::path> recipes(const std::filesystem::path &root);
// FNV-1a of canonical float WAV plus generator version; not a security hash.
std::string fingerprint(const PcmBuffer &pcm);
} // namespace gpp::audio::sfx
