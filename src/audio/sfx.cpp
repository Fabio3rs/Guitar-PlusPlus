#include "sfx.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace gpp::audio::sfx {
namespace {
constexpr double pi = 3.14159265358979323846;

void range(double value, double low, double high, const std::string &field) {
    if (!std::isfinite(value) || value < low || value > high) {
        throw Error(field + " must be finite and in [" + std::to_string(low) +
                    ", " + std::to_string(high) + "]");
    }
}

template <class T, size_t N>
const char *enumName(T value, const char *const (&names)[N]) {
    const auto index = static_cast<size_t>(value);
    if (index >= N) {
        throw Error("Invalid SFX enum value");
    }

    return names[index];
}

// xorshift32, upper 24 bits mapped exactly to [-1, 1). Seed zero is allowed.
struct Random {
    uint32_t state;
    explicit Random(uint32_t seed) : state(seed ? seed : 0x6d2b79f5u) {}

    double next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return double(state >> 8) * (1.0 / 8388608.0) - 1;
    }
};

struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    Biquad(const Filter &f, unsigned rate) {
        if (f.type == FilterType::None) {
            return;
        }

        const double w = 2 * pi * f.cutoffHz / rate;
        const double c = std::cos(w), alpha = std::sin(w) / (2 * f.q);
        const double a0 = 1 + alpha;
        if (f.type == FilterType::Lowpass) {
            b0 = b2 = (1 - c) / 2;
            b1 = 1 - c;
        } else if (f.type == FilterType::Highpass) {
            b0 = b2 = (1 + c) / 2;
            b1 = -(1 + c);
        } else {
            b0 = alpha;
            b1 = 0;
            b2 = -alpha;
        }

        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 = -2 * c / a0;
        a2 = (1 - alpha) / a0;
    }

    double process(double x) {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

double blep(double phase, double step) {
    if (phase < step) {
        const double t = phase / step;
        return 2 * t - t * t - 1;
    }

    if (phase > 1 - step) {
        const double t = (phase - 1) / step;
        return t * t + 2 * t + 1;
    }

    return 0;
}

double oscillator(Wave wave, double p, double step, double duty) {
    switch (wave) {
    case Wave::Sine:
        return std::sin(2 * pi * p);
    case Wave::Triangle: {
        // Bounded additive triangle: no harmonics above Nyquist.
        double result = 0;
        for (int h = 1; h <= 31 && h * step < .5; h += 2) {
            result += std::cos(2 * pi * h * p) / (h * h);
        }

        return result * (8 / (pi * pi));
    }

    case Wave::Sawtooth:
        return 2 * p - 1 - blep(p, step);
    case Wave::Square:
        duty = .5;
        [[fallthrough]];
    case Wave::Pulse: {
        double shifted = p - duty;
        shifted -= std::floor(shifted);
        return (p < duty ? 1 : -1) + blep(p, step) - blep(shifted, step);
    }
    }

    return 0;
}

double envelope(const Envelope &e, double t, double duration) {
    const double total = e.attack + e.decay + e.release;
    const double scale = total > duration ? duration / total : 1;
    const double attack = e.attack * scale, decay = e.decay * scale;
    const double release = e.release * scale;
    double gain = e.sustain;
    if (attack > 0 && t < attack) {
        gain = t / attack;
    } else if (decay > 0 && t < attack + decay) {
        const double x = (t - attack) / decay;
        const double shape =
            e.exponential
                ? (std::exp(-5 * x) - std::exp(-5.)) / (1 - std::exp(-5.))
                : 1 - x;
        gain = e.sustain + (1 - e.sustain) * shape;
    }

    if (release > 0 && t > duration - release) {
        const double x = std::clamp((duration - t) / release, 0., 1.);
        gain *= e.exponential ? x * x : x;
    }

    return gain;
}

void checkpointAt(const Checkpoint &cb, size_t frame, double progress) {
    if (cb && frame % 4096 == 0) {
        cb(progress);
    }
}
} // namespace

const char *name(Generator v) {
    const char *const n[] = {"oscillator", "noise", "pluck"};
    return enumName(v, n);
}

const char *name(Wave v) {
    const char *const n[] = {"sine", "triangle", "sawtooth", "square", "pulse"};
    return enumName(v, n);
}

const char *name(Color v) {
    const char *const n[] = {"white", "pink", "brown"};
    return enumName(v, n);
}

const char *name(FilterType v) {
    const char *const n[] = {"none", "lowpass", "highpass", "bandpass"};
    return enumName(v, n);
}

const char *name(Curve v) {
    const char *const n[] = {"linear", "exponential"};
    return enumName(v, n);
}

void validate(const SoundDefinition &s) {
    auto id = [](const std::string &text, const char *field) {
        if (text.empty() || text.size() > 64 ||
            !std::all_of(text.begin(), text.end(), [](unsigned char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9') || c == '_' || c == '-';
            })) {
            throw Error(std::string(field) +
                        " requires 1..64 ASCII letters, digits, _ or -");
        }
    };
    id(s.id, "id");
    id(s.category, "category");
    if (s.description.size() > 4096 ||
        s.description.find('\0') != std::string::npos) {
        throw Error("description exceeds 4096 bytes or contains NUL");
    }

    range(s.schemaVersion, 1, 1, "schemaVersion");
    range(s.sampleRate, 8000, 96000, "sampleRate");
    range(s.channels, 1, 2, "channels");
    range(s.duration, .001, 30, "duration");
    range(s.outputGainDb, -80, 12, "outputGainDb");
    range(s.peakLimit, .01, .99, "peakLimit");
    range(double(s.layers.size()), 1, maxLayers, "layers count");
    range(s.loop.begin, 0, s.duration, "loop.begin");
    range(s.loop.end, 0, s.duration, "loop.end");
    range(s.loop.crossfade, 0, 1, "loop.crossfade");
    const double end = s.loop.end == 0 ? s.duration : s.loop.end;
    const auto frames = size_t(std::llround(s.duration * s.sampleRate));
    const auto extra = size_t(std::llround(s.loop.crossfade * s.sampleRate));
    if (s.loop.enabled && (std::llround(end * s.sampleRate) <=
                               std::llround(s.loop.begin * s.sampleRate) + 1 ||
                           s.loop.crossfade > (end - s.loop.begin) / 2 ||
                           (s.loop.crossfade > 0 && extra < 2))) {
        throw Error("loop requires >= 2 frames; crossfade must be 0 or 2 "
                    "frames..half the loop");
    }

    if ((frames + extra) * s.channels > maxPcmBytes / sizeof(float)) {
        throw Error("duration/sampleRate/channels exceed 64 MiB PCM limit");
    }

    double work = 0;
    for (size_t i = 0; i < s.layers.size(); ++i) {
        const auto &l = s.layers[i];
        const std::string p = "layers[" + std::to_string(i + 1) + "].";
        name(l.type);
        name(l.waveform);
        name(l.color);
        name(l.frequency.curve);
        name(l.filter.type);
        range(l.start, 0, s.duration - 1. / s.sampleRate, p + "start");
        range(l.duration, 0, s.duration - l.start + 1e-10, p + "duration");
        const double length =
            l.duration == 0 ? s.duration - l.start : l.duration;
        range(length, 1. / s.sampleRate, 30, p + "duration");
        range(l.gain, 0, 4, p + "gain");
        range(l.pan, -1, 1, p + "pan");
        range(l.phase, 0, 1, p + "phase");
        range(l.duty, .05, .95, p + "duty");
        range(l.damping, .8, .9999, p + "damping");
        range(l.modulation.rateHz, 0, 20, p + "modulation.rateHz");
        range(l.modulation.pitchCents, 0, 1200, p + "modulation.pitchCents");
        range(l.modulation.amplitude, 0, 1, p + "modulation.amplitude");
        const double ratio = std::exp2(l.modulation.pitchCents / 1200);
        const double maxHz =
            s.sampleRate * (l.type == Generator::Pluck ? .125 : .45);
        range(l.frequency.startHz, 20 * ratio, maxHz / ratio,
              p + "frequency.startHz");
        range(l.frequency.endHz, 20 * ratio, maxHz / ratio,
              p + "frequency.endHz");
        range(l.envelope.attack, 0, 30, p + "envelope.attack");
        range(l.envelope.decay, 0, 30, p + "envelope.decay");
        range(l.envelope.sustain, 0, 1, p + "envelope.sustain");
        range(l.envelope.release, 0, 30, p + "envelope.release");
        range(l.filter.cutoffHz, 10, s.sampleRate * .45, p + "filter.cutoffHz");
        range(l.filter.q, .1, 10, p + "filter.q");
        if (l.enabled) {
            work += (length + s.loop.crossfade) * s.sampleRate;
        }
    }

    range(work, 0, 32000000, "total layer frames (duration/layers)");
}

PcmBuffer render(const SoundDefinition &s, const Checkpoint &cb) {
    validate(s);
    if (cb) {
        cb(0);
    }

    PcmBuffer pcm;
    pcm.rate = s.sampleRate;
    pcm.channels = s.channels;
    const size_t frames = size_t(std::llround(s.duration * pcm.rate));
    const size_t overlap =
        s.loop.enabled ? size_t(std::llround(s.loop.crossfade * pcm.rate)) : 0;
    pcm.samples.assign((frames + overlap) * pcm.channels, 0);
    if (s.loop.enabled) {
        pcm.loop = {
            size_t(std::llround(s.loop.begin * pcm.rate)),
            size_t(std::llround((s.loop.end == 0 ? s.duration : s.loop.end) *
                                pcm.rate))};
    }

    for (size_t li = 0; li < s.layers.size(); ++li) {
        const auto &l = s.layers[li];
        if (!l.enabled) {
            continue;
        }

        const size_t begin = size_t(std::llround(l.start * pcm.rate));
        const double duration =
            l.duration == 0 ? s.duration - l.start : l.duration;
        const size_t length =
            std::min(frames - begin, size_t(std::llround(duration * pcm.rate)));
        // Continue generators beyond the final frame for an ambient crossfade.
        // Finite layers that finish earlier are never extended.
        const size_t count = length + (begin + length == frames ? overlap : 0);
        Random random(s.seed ^ (0x9e3779b9u * uint32_t(li + 1)));
        Biquad filter(l.filter, pcm.rate);
        double phase = l.phase, pink[7] = {}, brown = 0, previous = 0;
        const double minHz = std::min(l.frequency.startHz, l.frequency.endHz) /
                             std::exp2(l.modulation.pitchCents / 1200);
        std::vector<double> delay;
        size_t write = 0;
        if (l.type == Generator::Pluck) {
            delay.resize(size_t(std::ceil(pcm.rate / minHz)) + 4);
            double mean = 0;
            for (auto &v : delay) {
                v = random.next();
                mean += v;
            }

            mean /= delay.size();
            for (auto &v : delay) {
                v -= mean;
            }
        }

        const double left = pcm.channels == 1 ? 1 : std::sqrt(.5 * (1 - l.pan));
        const double right = std::sqrt(.5 * (1 + l.pan));
        for (size_t i = 0; i < count; ++i) {
            checkpointAt(cb, i,
                         .8 * (li + double(i) / count) / s.layers.size());
            const double t = double(i) / pcm.rate;
            const double u = std::min(1., t / duration);
            double hz =
                l.frequency.curve == Curve::Linear
                    ? l.frequency.startHz +
                          (l.frequency.endHz - l.frequency.startHz) * u
                    : l.frequency.startHz *
                          std::pow(l.frequency.endHz / l.frequency.startHz, u);
            const double mod = std::sin(2 * pi * l.modulation.rateHz * t);
            hz *= std::exp2(mod * l.modulation.pitchCents / 1200);
            double v = 0;
            if (l.type == Generator::Oscillator) {
                v = oscillator(l.waveform, phase, hz / pcm.rate, l.duty);
                phase += hz / pcm.rate;
                phase -= std::floor(phase);
            } else if (l.type == Generator::Noise) {
                const double white = random.next();
                v = white;
                if (l.color == Color::Pink) {
                    pink[0] = .99886 * pink[0] + white * .0555179;
                    pink[1] = .99332 * pink[1] + white * .0750759;
                    pink[2] = .96900 * pink[2] + white * .1538520;
                    pink[3] = .86650 * pink[3] + white * .3104856;
                    pink[4] = .55000 * pink[4] + white * .5329522;
                    pink[5] = -.7616 * pink[5] - white * .0168980;
                    v = (pink[0] + pink[1] + pink[2] + pink[3] + pink[4] +
                         pink[5] + pink[6] + white * .5362) *
                        .11;
                    pink[6] = white * .115926;
                } else if (l.color == Color::Brown) {
                    brown = .995 * brown + .05 * white;
                    v = brown;
                }
            } else {
                double read = double(write) - (pcm.rate / hz - .5);
                if (read < 0) {
                    read += delay.size();
                }

                const size_t a = size_t(read) % delay.size();
                const double fraction = read - std::floor(read);
                v = delay[a] * (1 - fraction) +
                    delay[(a + 1) % delay.size()] * fraction;
                delay[write] = l.damping * .5 * (v + previous);
                previous = v;
                write = (write + 1) % delay.size();
            }

            // Hold the last envelope value only in the generated continuation.
            v = filter.process(v) *
                envelope(l.envelope, std::min(t, double(length - 1) / pcm.rate),
                         duration);
            v *= l.gain * (1 - l.modulation.amplitude * .5 * (1 - mod));
            pcm.samples[(begin + i) * pcm.channels] += float(v * left);
            if (pcm.channels == 2) {
                pcm.samples[(begin + i) * 2 + 1] += float(v * right);
            }
        }
    }

    // Remove DC with a 10 Hz blocker. State belongs to this render only.
    const double pole = std::exp(-2 * pi * 10 / pcm.rate);
    double x[2] = {}, y[2] = {};
    const double gain = std::pow(10., s.outputGainDb / 20);
    for (size_t i = 0; i < pcm.frames(); ++i) {
        checkpointAt(cb, i, .8 + .1 * double(i) / pcm.frames());
        for (unsigned ch = 0; ch < pcm.channels; ++ch) {
            auto &v = pcm.samples[i * pcm.channels + ch];
            const double next = v - x[ch] + pole * y[ch];
            x[ch] = v;
            y[ch] = next;
            v = float(next * gain);
        }
    }

    if (overlap) {
        const size_t begin = pcm.loop.begin, end = pcm.loop.end;
        // Blend a continued end into the head. The actual seam is between
        // consecutive generator samples end-1/end, even with random noise.
        for (size_t i = 0; i < overlap; ++i) {
            const double alpha =
                .5 - .5 * std::cos(pi * double(i) / (overlap - 1));
            for (unsigned ch = 0; ch < pcm.channels; ++ch) {
                auto &head = pcm.samples[(begin + i) * pcm.channels + ch];
                const auto continuation =
                    pcm.samples[(end + i) * pcm.channels + ch];
                head = float(continuation * (1 - alpha) + head * alpha);
            }
        }

        // For partial loops, approach the new head smoothly from the intro.
        const size_t intro = std::min(begin, overlap);
        for (size_t i = 0; i < intro; ++i) {
            const double alpha =
                intro > 1 ? .5 - .5 * std::cos(pi * double(i) / (intro - 1))
                          : 1;
            for (unsigned ch = 0; ch < pcm.channels; ++ch) {
                auto &v = pcm.samples[(begin - intro + i) * pcm.channels + ch];
                v = float(v * (1 - alpha) +
                          pcm.samples[(end - intro + i) * pcm.channels + ch] *
                              alpha);
            }
        }
    }

    pcm.samples.resize(frames * pcm.channels);
    if (!s.loop.enabled) {
        // A short final safety ramp also covers zero-time envelopes and phase.
        const size_t fade =
            std::min(frames / 2, std::max(size_t(2), size_t(.001 * pcm.rate)));
        for (size_t i = 0; i < fade; ++i) {
            const float a = float(i) / float(fade - 1);
            for (unsigned ch = 0; ch < pcm.channels; ++ch) {
                pcm.samples[i * pcm.channels + ch] *= a;
                pcm.samples[(frames - 1 - i) * pcm.channels + ch] *= a;
            }
        }
    }

    if (cb) {
        cb(.95);
    }

    const auto measured = levels(pcm); // Also rejects any non-finite output.
    if (measured.peak > s.peakLimit) {
        // Attenuate the entire mix only; never boost or normalize each layer.
        normalize(pcm, measured.rms, s.peakLimit);
    }

    if (cb) {
        cb(1);
    }

    return pcm;
}

std::string fingerprint(const PcmBuffer &pcm) {
    uint64_t hash = 14695981039346656037ull;
    for (unsigned i = 0; i < 4; ++i) {
        hash = (hash ^ uint8_t(generatorVersion >> (i * 8))) * 1099511628211ull;
    }

    for (auto byte : wavBytes(pcm, true)) {
        hash = (hash ^ byte) * 1099511628211ull;
    }

    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}
} // namespace gpp::audio::sfx
