#include "audio.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <random>

namespace gpp::audio {
namespace {
constexpr double pi = 3.14159265358979323846;
double frequency(int midi) { return 440 * std::pow(2., (midi - 69) / 12.); }

std::string noteName(int midi) {
    static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                  "F#", "G",  "G#", "A",  "A#", "B"};
    return std::string(names[midi % 12]) + std::to_string(midi / 12 - 1);
}

float band(const std::vector<float> &spectrum, double hz, unsigned rate,
           int fft, double cents) {
    if (hz <= 0 || hz >= rate * .5) {
        return 0;
    }
    const int low = std::max(
        1, int(std::floor(hz * std::pow(2., -cents / 1200.) * fft / rate)));
    const int high =
        std::min(int(spectrum.size()) - 1,
                 int(std::ceil(hz * std::pow(2., cents / 1200.) * fft / rate)));
    return low > high ? 0
                      : *std::max_element(spectrum.begin() + low,
                                          spectrum.begin() + high + 1);
}
} // namespace

AnalysisResult analyze(const std::filesystem::path &path, double time,
                       const AnalysisSettings &s,
                       const Checkpoint &checkpoint) {
    Settings validation;
    validation.analysis = s;
    validate(validation);
    const auto start = std::chrono::steady_clock::now();
    auto stream = openFile(path, true);
    const auto info = sourceInfo(stream.get());
    if (!std::isfinite(time) || time < 0 || time >= info.duration) {
        throw Error("Analysis time must be inside the source");
    }
    AnalysisResult result;
    result.rate = info.rate;
    result.spectrum.assign(size_t(s.fft / 2), 0);
    std::vector<float> frame(result.spectrum.size());
    const int wanted = std::max(
        1, int(std::ceil(std::min(s.window, info.duration - time) / s.hop)));
    DWORD fftFlag = BASS_DATA_FFT512;
    for (int n = 512; n < s.fft; n *= 2) {
        ++fftFlag;
    }
    int obtained = 0;
    for (int i = 0; i < wanted; ++i) {
        if (checkpoint) {
            checkpoint(double(i) / wanted);
        }
        const double at = time + i * s.hop;
        if (at >= info.duration) {
            break;
        }
        seek(stream.get(), at);
        std::fill(frame.begin(), frame.end(), 0);
        const DWORD got =
            BASS_ChannelGetData(stream.get(), frame.data(), fftFlag);
        if (got == DWORD(-1)) {
            if (BASS_ErrorGetCode() == BASS_ERROR_ENDED) {
                break;
            }
            check(false, "Read FFT");
        }
        for (size_t bin = 1; bin < frame.size(); ++bin) {
            result.spectrum[bin] += std::sqrt(std::max(0.f, frame[bin]));
        }
        ++obtained;
    }
    if (!obtained) {
        throw Error("No FFT frames available at this position");
    }
    for (auto &value : result.spectrum) {
        value /= float(obtained);
    }
    std::vector<Candidate> scored;
    for (int midi = s.midiMin; midi <= s.midiMax; ++midi) {
        const double hz = frequency(midi);
        if (hz >= info.rate * .5) {
            continue;
        }
        double score = 0, weights = 0;
        for (int h = 1; h <= s.harmonics; ++h) {
            if (hz * h >= std::min(info.rate * .48, 6000.)) {
                break;
            }
            const double weight = 1 / std::pow(double(h), s.harmonicFalloff);
            score += weight *
                     band(result.spectrum, hz * h, info.rate, s.fft, s.cents);
            weights += weight;
        }
        scored.push_back(
            {midi, hz, weights ? score / weights : 0, noteName(midi)});
    }
    double maximum = 0;
    for (const auto &candidate : scored) {
        maximum = std::max(maximum, candidate.score);
    }
    std::vector<Candidate> local;
    if (maximum > 1e-8) {
        for (size_t i = 0; i < scored.size(); ++i) {
            const double previous = i ? scored[i - 1].score : -1;
            const double next =
                i + 1 < scored.size() ? scored[i + 1].score : -1;
            if (scored[i].score >= previous && scored[i].score >= next &&
                scored[i].score >= maximum * s.threshold) {
                local.push_back(scored[i]);
            }
        }
    }
    std::sort(local.begin(), local.end(),
              [](const Candidate &a, const Candidate &b) {
                  return a.score > b.score;
              });
    for (const auto &candidate : local) {
        bool reject = false;
        for (const auto &chosen : result.notes) {
            for (double harmonic : {2., 3., 4.}) {
                if (std::abs(1200 * std::log2(candidate.frequency /
                                              chosen.frequency / harmonic)) <
                        s.cents &&
                    candidate.score < chosen.score * s.pruneRatio) {
                    reject = true;
                }
            }
        }
        if (!reject) {
            result.notes.push_back(candidate);
        }
        if (int(result.notes.size()) == s.candidates) {
            break;
        }
    }
    std::sort(
        result.notes.begin(), result.notes.end(),
        [](const Candidate &a, const Candidate &b) { return a.midi < b.midi; });
    result.milliseconds = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - start)
                              .count();
    return result;
}

PcmBuffer synthesize(const AnalysisResult &analysis, const SynthSettings &s,
                     const Checkpoint &checkpoint) {
    Settings validation;
    validation.synth = s;
    validate(validation);
    auto notes = analysis.notes;
    for (const auto &note : notes) {
        if (note.midi < 0 || note.midi > 127 || !std::isfinite(note.score) ||
            note.score < 0) {
            throw Error("Invalid synthesis candidate");
        }
    }
    std::sort(notes.begin(), notes.end(),
              [](const Candidate &a, const Candidate &b) {
                  return a.score > b.score;
              });
    struct Voice {
        double hz, phase, pan, gain;
    };
    std::vector<Voice> voices;
    std::mt19937 rng(0xF00D1234u);
    std::uniform_real_distribution<double> detune(-s.detune, s.detune),
        phase(0, 2 * pi), noise(-1, 1);
    int pressed = 0;
    for (int fret = 0; fret < 5; ++fret) {
        pressed += (s.fretMask >> fret) & 1;
    }
    if (notes.empty() || !pressed) {
        // Prototype's unfretted/silent-analysis fallback: muted E2/B2 thunk.
        notes = {{40, frequency(40), 1, "E2"}, {47, frequency(47), .7, "B2"}};
        for (size_t i = 0; i < notes.size() && int(voices.size()) < s.voices;
             ++i) {
            voices.push_back(
                {notes[i].frequency * std::pow(2., detune(rng) / 1200.),
                 phase(rng), 0, 1});
        }
    } else {
        const int perFret = std::max(1, s.voices / pressed);
        for (int fret = 0; fret < 5 && int(voices.size()) < s.voices; ++fret) {
            if (!(s.fretMask & (1 << fret))) {
                continue;
            }
            for (size_t n = 0; n < notes.size() && int(n) < perFret &&
                               int(voices.size()) < s.voices;
                 ++n) {
                const int midi =
                    std::clamp(notes[n].midi + s.shifts[fret], 0, 127);
                voices.push_back(
                    {frequency(midi) * std::pow(2., detune(rng) / 1200.),
                     phase(rng), 0,
                     s.gains[fret] * std::max(.1, 1 - .06 * double(n))});
            }
        }
    }
    for (size_t i = 0; i < voices.size(); ++i) {
        voices[i].pan =
            voices.size() == 1
                ? 0
                : -s.spread + 2 * s.spread * double(i) / (voices.size() - 1);
    }
    PcmBuffer result;
    result.rate = unsigned(s.rate);
    const double duration =
        s.loop ? s.loopStart + s.loopBody + s.release : s.duration;
    result.samples.assign(size_t(std::ceil(duration * s.rate)) * 2, 0);
    if (s.loop) {
        result.loop = {size_t(s.loopStart * s.rate),
                       size_t((s.loopStart + s.loopBody) * s.rate)};
    }
    const double attack = std::min(s.attack, duration * .25);
    const double fadeStart = std::min(s.fadeStart, duration * .5);
    double sumGain = 0;
    for (const auto &voice : voices) {
        sumGain += voice.gain;
    }
    const double scale = .8 / std::max(1., sumGain);
    for (size_t frame = 0; frame < result.frames(); ++frame) {
        if ((frame % 4096) == 0 && checkpoint) {
            checkpoint(double(frame) / result.frames());
        }
        const double t = double(frame) / s.rate;
        const double decayTime = s.loop ? std::min(t, s.loopStart) : t;
        double envelope =
            std::min(1., t / attack) *
            (s.sustain + (1 - s.sustain) * std::exp(-decayTime / s.decay));
        const double releaseStart =
            s.loop ? s.loopStart + s.loopBody : fadeStart;
        if (t > releaseStart) {
            const double remaining =
                std::clamp((duration - t) / (duration - releaseStart), 0., 1.);
            envelope *= remaining * remaining;
        }
        const double transient =
            s.transient * noise(rng) * std::exp(-t * s.transientDecay);
        for (const auto &voice : voices) {
            const double hz =
                voice.hz * (1 - s.bend * std::exp(-t * s.bendDecay));
            double value = 0;
            for (int h = 1; h <= s.harmonics && hz * h < s.rate * .5; ++h) {
                value += std::sin(2 * pi * hz * h * t + voice.phase * h) /
                         std::pow(double(h), s.harmonicFalloff);
            }
            value = (value * .18 + transient / voices.size()) * envelope *
                    scale * voice.gain;
            result.samples[frame * 2] +=
                float(value * std::sqrt(.5 * (1 - voice.pan)));
            result.samples[frame * 2 + 1] +=
                float(value * std::sqrt(.5 * (1 + voice.pan)));
        }
        for (size_t ch = 0; ch < 2; ++ch) {
            auto &sample = result.samples[frame * 2 + ch];
            sample = float(std::tanh(sample * s.saturation) /
                           std::tanh(s.saturation));
        }
    }
    if (s.normalize) {
        normalize(result, s.rms, s.peak);
    }
    return result;
}

void bakeLoop(PcmBuffer &pcm, double crossfade) {
    if (!std::isfinite(crossfade) || crossfade < 0 || crossfade > 1) {
        throw Error("Invalid loop crossfade");
    }
    if (!pcm.loop.end) {
        return;
    }
    if (!pcm.channels || pcm.loop.begin >= pcm.loop.end ||
        pcm.loop.end > pcm.frames()) {
        throw Error("Invalid PCM loop bounds");
    }
    const size_t length = pcm.loop.end - pcm.loop.begin;
    const size_t frames =
        std::min(size_t(crossfade * pcm.rate), (length - 1) / 2);
    const size_t tail = pcm.loop.end - frames;
    for (size_t frame = 0; frame < frames; ++frame) {
        const float alpha = float(frame) / float(frames);
        for (unsigned ch = 0; ch < pcm.channels; ++ch) {
            auto &head =
                pcm.samples[(pcm.loop.begin + frame) * pcm.channels + ch];
            head =
                pcm.samples[(tail + frame) * pcm.channels + ch] * (1 - alpha) +
                head * alpha;
        }
    }
    // Preserve the prototype's baked-loop semantics: the overlapped tail and
    // release are excluded. A loop preview stops immediately on Stop.
    pcm.loop.end -= frames;
    pcm.samples.resize(pcm.loop.end * pcm.channels);
}
} // namespace gpp::audio
