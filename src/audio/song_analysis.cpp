#include "song_analysis.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace gpp::audio {
namespace {
using Clock = std::chrono::steady_clock;
double ms(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}
void progress(const Checkpoint &cb, double p) {
    if (cb) {
        cb(p);
    }
}
template <size_t N> void normalizeDistribution(std::array<float, N> &v) {
    double sum = std::accumulate(v.begin(), v.end(), 0.);
    if (sum > 0) {
        for (auto &x : v) {
            x = float(x / sum);
        }
    }
}
template <size_t N>
double cosine(const std::array<float, N> &a, const std::array<float, N> &b) {
    double dot = 0, aa = 0, bb = 0;
    for (size_t i = 0; i < N; ++i) {
        dot += double(a[i]) * b[i];
        aa += double(a[i]) * a[i];
        bb += double(b[i]) * b[i];
    }
    if (aa == 0 || bb == 0) {
        return aa == bb ? 0 : 1;
    }
    return std::clamp(1 - dot / std::sqrt(aa * bb), 0., 1.);
}
void blend(Signature &a, const Signature &b, double weight) {
    for (size_t i = 0; i < a.chroma.size(); ++i) {
        a.chroma[i] = float(a.chroma[i] * (1 - weight) + b.chroma[i] * weight);
    }
    for (size_t i = 0; i < a.bands.size(); ++i) {
        a.bands[i] = float(a.bands[i] * (1 - weight) + b.bands[i] * weight);
    }
}
struct WindowReader {
    const float *samples = nullptr;
    size_t bytes = 0, cursor = 0;
    static DWORD CALLBACK read(HSTREAM, void *buffer, DWORD requested,
                               void *user) {
        auto &r = *static_cast<WindowReader *>(user);
        const auto count = std::min(size_t(requested), r.bytes - r.cursor);
        std::memcpy(buffer,
                    reinterpret_cast<const char *>(r.samples) + r.cursor,
                    count);
        r.cursor += count;
        return DWORD(count);
    }
};
void describe(FailRegion &region, const SongAnalysis &scan) {
    region.signature = {};
    size_t count = 0;
    const auto first = std::lower_bound(
        scan.observations.begin(), scan.observations.end(), region.begin,
        [](const auto &o, uint64_t frame) { return o.frame < frame; });
    const auto last = std::lower_bound(
        first, scan.observations.end(), region.end,
        [](const auto &o, uint64_t frame) { return o.frame < frame; });
    for (auto it = first; it != last; ++it) {
        const auto &o = *it;
        if (o.frame >= region.begin && o.frame < region.end &&
            o.rms >= std::pow(10., scan.settings.silenceDb / 20)) {
            blend(region.signature, o.signature, 1. / ++count);
        }
    }
    region.representative = region.begin;
    double best = 1e100;
    for (auto it = first; it != last; ++it) {
        const auto &o = *it;
        if (o.frame < region.begin || o.frame >= region.end ||
            o.rms < std::pow(10., scan.settings.silenceExitDb / 20)) {
            continue;
        }
        const double score = distance(o.signature, region.signature,
                                      scan.settings.harmonicWeight) +
                             .25 * o.variation;
        // Prefer enough room for the default point analysis, without crossing a
        // boundary.
        const double edge =
            (region.end - o.frame < uint64_t(.4 * scan.info.rate)) ? 1 : 0;
        if (score + edge < best) {
            best = score + edge;
            region.representative = o.frame;
        }
    }
}
} // namespace

void validate(const SongSettings &s) {
    auto copy = s;
    visitSongSettings(
        copy, [](const std::string &name, auto &v, double lo, double hi) {
            if (!std::isfinite(double(v)) || double(v) < lo || double(v) > hi) {
                throw Error("Invalid " + name);
            }
        });
    if ((s.fft & (s.fft - 1)) || s.hysteresis > s.threshold ||
        s.silenceDb > s.silenceExitDb) {
        throw Error("Invalid FFT, hysteresis or silence thresholds");
    }
}

double distance(const Signature &a, const Signature &b, double w) {
    return w * cosine(a.chroma, b.chroma) + (1 - w) * cosine(a.bands, b.bands);
}

SourceIdentity identifySource(const std::filesystem::path &path,
                              const Checkpoint &checkpoint) {
    SourceIdentity result;
    result.path = std::filesystem::absolute(path).string();
    const auto expected = std::filesystem::file_size(path);
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw Error("Cannot identify audio source");
    }
    std::array<char, 65536> data{};
    uint64_t hash = 14695981039346656037ull;
    while (in) {
        progress(checkpoint, expected ? double(result.bytes) / expected : 0);
        in.read(data.data(), data.size());
        for (std::streamsize i = 0; i < in.gcount(); ++i) {
            hash = (hash ^ uint8_t(data[size_t(i)])) * 1099511628211ull;
        }
        result.bytes += uint64_t(in.gcount());
    }
    if (!in.eof() || result.bytes != expected) {
        throw Error("Source changed or could not be read");
    }
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    result.hash = out.str();
    return result;
}

SongAnalysis scanSong(const std::filesystem::path &path, const SongSettings &s,
                      const Checkpoint &checkpoint) {
    validate(s);
    SongAnalysis result;
    result.settings = s;
    result.bassVersion = BASS_GetVersion();
    const auto start = Clock::now();
    result.source =
        identifySource(path, [&](double p) { progress(checkpoint, p * .05); });
    result.identifyMs = ms(start);
    auto stage = Clock::now();
    auto file = openFile(path, true);
    result.info = sourceInfo(file.get());
    result.frames =
        uint64_t(std::llround(result.info.duration * result.info.rate));
    if (!result.frames || result.info.channels > 8 ||
        result.info.rate > 384000) {
        throw Error("Unsupported source format");
    }
    const auto chans = result.info.channels;
    const size_t n = size_t(s.fft),
                 hop = std::max(size_t(1),
                                size_t(std::llround(s.hop * result.info.rate)));
    const auto observationCount = (result.frames + hop - 1) / hop;
    if (observationCount > 2000000) {
        throw Error("Too many observations; increase song.hop");
    }
    result.observations.reserve(size_t(observationCount));
    std::vector<float> window(n * chans),
        scratch(std::max(size_t(4096), std::min(hop, size_t(65536))) * chans);
    std::vector<float> spectrum(n / 2 * chans);
    WindowReader reader{window.data(), window.size() * sizeof(float), 0};
    Stream fft(BASS_StreamCreate(result.info.rate, chans,
                                 BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE,
                                 WindowReader::read, &reader));
    check(fft.get() != 0, "Create window FFT stream");
    DWORD flag = BASS_DATA_FFT512 | BASS_DATA_FFT_INDIVIDUAL;
    for (int size = 512; size < s.fft; size *= 2) {
        ++flag;
    }
    result.openMs = ms(stage);
    size_t valid = 0;
    bool ended = false;
    auto decode = [&](float *out, size_t frames) {
        auto t = Clock::now();
        size_t total = 0;
        while (total < frames && !ended) {
            progress(checkpoint,
                     .05 + .9 * double(result.observations.size()) /
                               std::max(uint64_t(1), observationCount));
            const auto count = std::min(size_t(4096), frames - total);
            DWORD got =
                BASS_ChannelGetData(file.get(), out + total * chans,
                                    DWORD(count * chans * sizeof(float)));
            if (got == DWORD(-1)) {
                if (BASS_ErrorGetCode() != BASS_ERROR_ENDED) {
                    check(false, "Decode song scan");
                }
                ended = true;
                break;
            }
            if (!got) {
                ended = true;
                break;
            }
            if (got % (chans * sizeof(float))) {
                throw Error("Unaligned decoded PCM");
            }
            total += got / sizeof(float) / chans;
        }
        result.decodeMs += ms(t);
        return total;
    };
    valid = decode(window.data(), n);
    uint64_t frame = 0;
    while (frame < result.frames && valid) {
        std::fill(window.begin() + valid * chans, window.end(), 0);
        reader.cursor = 0;
        stage = Clock::now();
        check(BASS_ChannelGetData(fft.get(), spectrum.data(), flag) !=
                  DWORD(-1),
              "Window FFT");
        result.fftMs += ms(stage);
        stage = Clock::now();
        Observation o;
        o.frame = frame;
        double energy = 0;
        for (size_t i = 0; i < valid * chans; ++i) {
            energy += double(window[i]) * window[i];
        }
        o.rms = std::sqrt(energy / (valid * chans));
        for (size_t bin = 1; bin < n / 2; ++bin) {
            const double hz = double(bin) * result.info.rate / n;
            if (hz < 55 || hz > 8000) {
                continue;
            }
            double power = 0;
            for (size_t ch = 0; ch < chans; ++ch) {
                const auto x = spectrum[bin * chans + ch];
                power += double(x) * x;
            }
            const int band = std::clamp(
                int(std::log(hz / 55) / std::log(8000. / 55) * 24), 0, 23);
            o.signature.bands[size_t(band)] += float(power);
            if (hz <= 3520) {
                const double midi = 69 + 12 * std::log2(hz / 440);
                const int low = int(std::floor(midi));
                const double fraction = midi - low;
                o.signature.chroma[size_t((low % 12 + 12) % 12)] +=
                    float(power * (1 - fraction));
                o.signature.chroma[size_t(((low + 1) % 12 + 12) % 12)] +=
                    float(power * fraction);
            }
        }
        // Store volume-independent raw distributions; silence is applied during
        // segmentation.
        normalizeDistribution(o.signature.chroma);
        normalizeDistribution(o.signature.bands);
        if (!result.observations.empty()) {
            o.variation =
                distance(o.signature, result.observations.back().signature,
                         s.harmonicWeight);
        }
        result.observations.push_back(o);
        result.descriptorMs += ms(stage);
        if (hop < n) {
            const auto retained = valid > hop ? valid - hop : 0;
            std::move(window.begin() + std::min(valid, hop) * chans,
                      window.begin() + valid * chans, window.begin());
            valid = retained +
                    decode(window.data() + retained * chans, n - retained);
        } else {
            size_t skip = hop - n;
            while (skip && !ended) {
                const auto got = decode(scratch.data(),
                                        std::min(skip, scratch.size() / chans));
                skip -= got;
            }
            valid = decode(window.data(), n);
        }
        frame += hop;
    }
    if (result.observations.empty()) {
        throw Error("No audio observations");
    }
    const auto finalIdentity = identifySource(
        path, [&](double p) { progress(checkpoint, .95 + .04 * p); });
    if (finalIdentity.hash != result.source.hash ||
        finalIdentity.bytes != result.source.bytes) {
        throw Error("Audio changed during scan");
    }
    result.scanMs = ms(start);
    result = segmentSong(result, s, [&](double) { progress(checkpoint, .99); });
    progress(checkpoint, 1);
    return result;
}

SongAnalysis segmentSong(const SongAnalysis &scan, const SongSettings &s,
                         const Checkpoint &checkpoint) {
    validate(s);
    if (scan.settings.fft != s.fft || scan.settings.hop != s.hop) {
        throw Error("FFT/hop changed; scan again");
    }
    if (scan.observations.empty() || !scan.frames || !scan.info.rate) {
        throw Error("No scan available");
    }
    const auto start = Clock::now();
    SongAnalysis out = scan;
    out.settings = s;
    for (size_t i = 1; i < out.observations.size(); ++i) {
        out.observations[i].variation =
            distance(out.observations[i - 1].signature,
                     out.observations[i].signature, s.harmonicWeight);
    }
    out.regions.clear();
    const double enter = std::pow(10., s.silenceDb / 20),
                 leave = std::pow(10., s.silenceExitDb / 20);
    bool silent = scan.observations.front().rms < enter;
    Signature smooth = scan.observations.front().signature, reference = smooth;
    size_t accepted = 1, candidate = 0;
    bool pending = false, candidateSilent = silent;
    FailRegion current;
    current.silent = silent;
    for (size_t i = 1; i < scan.observations.size() && !s.fixed; ++i) {
        progress(checkpoint, double(i) / scan.observations.size());
        const auto &o = scan.observations[i];
        const double dt =
            double(o.frame - scan.observations[i - 1].frame) / scan.info.rate;
        const bool nowSilent = o.rms < (silent ? leave : enter);
        if (!nowSilent) {
            blend(smooth, o.signature, 1 - std::exp(-dt / s.smoothing));
        }
        const double d =
            nowSilent != silent
                ? 1
                : (silent ? 0 : distance(smooth, reference, s.harmonicWeight));
        if (d >= s.threshold) {
            if (!pending || candidateSilent != nowSilent) {
                candidate = i;
                pending = true;
                candidateSilent = nowSilent;
            }
        } else if (d < s.hysteresis) {
            pending = false;
        }
        if (pending && double(o.frame - scan.observations[candidate].frame) /
                               scan.info.rate >=
                           s.persistence) {
            const auto boundary = scan.observations[candidate].frame;
            if (boundary > current.begin &&
                (nowSilent != silent ||
                 double(boundary - current.begin) / scan.info.rate >=
                     s.minimum)) {
                current.end = boundary;
                out.regions.push_back(current);
                current = {};
                current.begin = boundary;
                current.silent = candidateSilent;
                current.change = d;
                silent = candidateSilent;
                reference = smooth;
                accepted = 1;
                pending = false;
            }
        }
        if (!pending && !nowSilent) {
            blend(reference, smooth, 1. / ++accepted);
        }
    }
    current.end = scan.frames;
    if (s.fixed) {
        current.silent =
            std::none_of(scan.observations.begin(), scan.observations.end(),
                         [&](const auto &o) { return o.rms >= leave; });
    }
    out.regions.push_back(current);
    out.detectedRegions = out.regions.size();
    for (auto &region : out.regions) {
        describe(region, out);
    }
    for (size_t i = 1; i < out.regions.size();) {
        auto &left = out.regions[i - 1];
        auto &right = out.regions[i];
        if (left.silent == right.silent &&
            (left.silent ||
             distance(left.signature, right.signature, s.harmonicWeight) <=
                 s.merge ||
             double(left.end - left.begin) / scan.info.rate < s.minimum ||
             double(right.end - right.begin) / scan.info.rate < s.minimum)) {
            left.end = right.end;
            describe(left, out);
            out.regions.erase(out.regions.begin() + std::ptrdiff_t(i));
        } else {
            ++i;
        }
    }
    std::vector<size_t> anchors;
    for (size_t i = 0; i < out.regions.size(); ++i) {
        auto &r = out.regions[i];
        size_t best = anchors.size();
        double bestDistance = 2;
        for (size_t j = 0; j < anchors.size(); ++j) {
            const auto &a = out.regions[anchors[j]];
            const double d =
                distance(r.signature, a.signature, s.harmonicWeight);
            if (a.silent == r.silent &&
                (r.silent || (s.deduplicate && d <= s.dedupTolerance)) &&
                d < bestDistance) {
                best = j;
                bestDistance = d;
            }
        }
        if (best == anchors.size()) {
            anchors.push_back(i);
        }
        r.setId = uint32_t(best);
    }
    out.segmentMs = ms(start);
    validate(out);
    progress(checkpoint, 1);
    return out;
}

void validate(const SongAnalysis &a) {
    validate(a.settings);
    if (!a.info.rate || a.info.rate > 384000 || !a.info.channels ||
        a.info.channels > 8 || !a.frames || !std::isfinite(a.info.duration) ||
        a.info.duration <= 0 || a.regions.empty() ||
        a.regions.size() > 2000000 || a.observations.size() > 2000000 ||
        std::abs(double(a.frames) / a.info.rate - a.info.duration) >
            1. / a.info.rate) {
        throw Error("Invalid song analysis");
    }
    auto signature = [](const Signature &s) {
        for (float x : s.chroma) {
            if (!std::isfinite(x) || x < 0 || x > 1.00001f) {
                throw Error("Invalid chroma distribution");
            }
        }
        for (float x : s.bands) {
            if (!std::isfinite(x) || x < 0 || x > 1.00001f) {
                throw Error("Invalid spectral distribution");
            }
        }
    };
    uint64_t previous = 0;
    for (size_t i = 0; i < a.observations.size(); ++i) {
        const auto &o = a.observations[i];
        if (o.frame >= a.frames || (i && o.frame <= previous) ||
            (!i && o.frame != 0) || !std::isfinite(o.rms) || o.rms < 0 ||
            !std::isfinite(o.variation) || o.variation < 0 || o.variation > 1) {
            throw Error("Invalid observations");
        }
        previous = o.frame;
        signature(o.signature);
    }
    for (double value : {a.identifyMs, a.openMs, a.decodeMs, a.fftMs,
                         a.descriptorMs, a.scanMs, a.segmentMs}) {
        if (!std::isfinite(value) || value < 0) {
            throw Error("Invalid analysis metrics");
        }
    }
    uint64_t end = 0;
    for (const auto &r : a.regions) {
        if (r.begin != end || r.end <= r.begin || r.end > a.frames ||
            r.representative < r.begin || r.representative >= r.end ||
            !std::isfinite(r.change) || r.change < 0 || r.change > 1 ||
            r.setId >= a.regions.size()) {
            throw Error("Invalid region coverage");
        }
        signature(r.signature);
        end = r.end;
    }
    if (end != a.frames) {
        throw Error("Incomplete region coverage");
    }
}
} // namespace gpp::audio
