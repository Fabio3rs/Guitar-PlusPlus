#include "alignment.h"
#include <algorithm>
#include <fstream>
#include <numeric>
#include <stdexcept>

namespace ChartStudio {
namespace {
struct Feature {
    double time = 0, onset = 0;
    std::array<double, 12> value{};
    bool active = false;
};
std::vector<Feature> features(const ChromaSequence &input) {
    if (input.size() < 2 || input.size() > 200000) {
        throw std::runtime_error("Alignment needs 2..200000 observations");
    }
    std::vector<Feature> out;
    for (const auto &frame : input) {
        if (!std::isfinite(frame.time) || frame.time < 0 ||
            (!out.empty() && frame.time <= out.back().time)) {
            throw std::runtime_error("Invalid chroma timestamps");
        }
        Feature f;
        f.time = frame.time;
        double sum = 0;
        for (double v : frame.chroma) {
            if (!std::isfinite(v) || v < 0) {
                throw std::runtime_error("Invalid chroma energy");
            }
            sum += v;
        }
        if (sum > 1e-12) {
            double norm = 0;
            for (size_t j = 0; j < 12; ++j) {
                f.value[j] = frame.chroma[j] / sum - 1.0 / 12;
                norm += f.value[j] * f.value[j];
            }
            norm = std::sqrt(norm);
            f.active = norm > 1e-6;
            if (f.active) {
                for (auto &v : f.value) {
                    v /= norm;
                }
            }
        }
        if (f.active && !out.empty()) {
            double dot = 0;
            for (size_t j = 0; j < 12; ++j) {
                dot += f.value[j] * out.back().value[j];
            }
            f.onset = std::clamp(1 - dot, 0.0, 1.0);
        }
        out.push_back(f);
    }
    return out;
}
struct Scorer {
    const std::vector<Feature> &midi, &audio;
    // Audio scanner and score descriptors are uniform grids. Verify that rather
    // than silently indexing irregular inputs as if they were uniform.
    double hop;
    explicit Scorer(const std::vector<Feature> &m,
                    const std::vector<Feature> &a)
        : midi(m), audio(a), hop(a[1].time - a[0].time) {
        for (size_t i = 1; i < a.size(); ++i) {
            if (std::abs(a[i].time - a[0].time - double(i) * hop) > 1e-5) {
                throw std::runtime_error(
                    "Audio chroma must have uniform spacing");
            }
        }
    }
    AlignmentCandidate operator()(double scale, double offset,
                                  size_t stride = 1, double begin = 0,
                                  double end = 1e9) const {
        double sum = 0;
        size_t active = 0, covered = 0;
        for (size_t i = 0; i < midi.size(); i += stride) {
            const auto &m = midi[i];
            if (!m.active || m.time < begin || m.time >= end) {
                continue;
            }
            ++active;
            const double at =
                (scale * m.time + offset - audio.front().time) / hop;
            if (at < 0 || at > double(audio.size() - 1)) {
                continue;
            }
            ++covered;
            const auto &a = audio[size_t(std::llround(at))];
            double dot = 0;
            for (size_t j = 0; j < 12; ++j) {
                dot += m.value[j] * a.value[j];
            }
            sum += .9 * dot + .1 * m.onset * a.onset;
        }
        return {scale, offset, active ? sum / double(active) : 0,
                active ? double(covered) / double(active) : 0};
    }
};
bool better(const AlignmentCandidate &a, const AlignmentCandidate &b) {
    if (a.score != b.score) {
        return a.score > b.score;
    }
    if (std::abs(a.scale - 1) != std::abs(b.scale - 1)) {
        return std::abs(a.scale - 1) < std::abs(b.scale - 1);
    }
    return a.offset < b.offset;
}
bool distinct(const AlignmentCandidate &a, const AlignmentCandidate &b,
              double duration) {
    return std::max(std::abs(a.offset - b.offset),
                    std::abs((a.scale - b.scale) * duration + a.offset -
                             b.offset)) > .5;
}
std::vector<AlignmentCandidate> peaks(std::vector<AlignmentCandidate> all,
                                      double duration, size_t count) {
    std::sort(all.begin(), all.end(), better);
    std::vector<AlignmentCandidate> out;
    for (const auto &c : all) {
        if (std::all_of(out.begin(), out.end(), [&](const auto &p) {
                return distinct(c, p, duration);
            })) {
            out.push_back(c);
            if (out.size() == count) {
                break;
            }
        }
    }
    return out;
}
} // namespace

void AlignmentSettings::validate() const {
    if ((mode != "offset" && mode != "linear" && mode != "auto") ||
        !std::isfinite(minimumOffset) || !std::isfinite(maximumOffset) ||
        minimumOffset > maximumOffset || minimumOffset < -120 ||
        maximumOffset > 120 || !std::isfinite(minimumScale) ||
        !std::isfinite(maximumScale) || minimumScale < .9 ||
        maximumScale > 1.1 || minimumScale > 1 || maximumScale < 1) {
        throw std::runtime_error(
            "Invalid alignment search (offset/linear/auto; offsets +/-120 s; "
            "scales .9..1.1 including 1). DTW is not implemented.");
    }
}

ChromaSequence scoreChroma(const Score &score, double hop) {
    score.validate();
    if (!std::isfinite(hop) || hop < .02 || hop > .5) {
        throw std::runtime_error("Invalid chroma hop");
    }
    double end = 0;
    for (const auto &track : score.tracks) {
        for (const auto &n : track.notes) {
            if (n.channel != 9) {
                end = std::max(end, score.seconds(n.end));
            }
        }
    }
    if (end <= 0 || end / hop > 199998) {
        throw std::runtime_error(
            "Empty or excessive musical duration for alignment");
    }
    ChromaSequence out(size_t(std::ceil(end / hop)) + 1);
    for (size_t i = 0; i < out.size(); ++i) {
        out[i].time = double(i) * hop;
    }
    // Difference arrays avoid work proportional to every sustain's duration.
    std::vector<std::array<double, 12>> delta(out.size() + 1);
    for (const auto &track : score.tracks) {
        for (const auto &n : track.notes) {
            if (n.channel == 9) {
                continue;
            }
            const auto begin = std::min(
                out.size(), size_t(std::ceil(score.seconds(n.start) / hop)));
            const auto finish = std::min(
                out.size(),
                std::max(begin + 1,
                         size_t(std::ceil(score.seconds(n.end) / hop))));
            const double role = (n.program >= 24 && n.program <= 31)   ? 1.0
                                : (n.program >= 32 && n.program <= 39) ? .65
                                                                       : .8;
            const double weight = role * std::sqrt(double(n.velocity) / 127);
            delta[begin][size_t(n.pitch % 12)] += weight;
            delta[finish][size_t(n.pitch % 12)] -= weight;
        }
    }
    std::array<double, 12> current{};
    for (size_t i = 0; i < out.size(); ++i) {
        for (size_t j = 0; j < 12; ++j) {
            current[j] += delta[i][j];
            out[i].chroma[j] = std::max(0.0, current[j]);
        }
    }
    return out;
}

AlignmentResult alignChroma(const ChromaSequence &midi,
                            const ChromaSequence &audio,
                            const AlignmentSettings &settings,
                            const std::function<void()> &checkpoint) {
    settings.validate();
    const auto m = features(midi), a = features(audio);
    Scorer score(m, a);
    const double duration = m.back().time;
    const size_t stride = std::max(size_t(1), m.size() / 1200);
    std::vector<AlignmentCandidate> coarse;
    auto check = [&] {
        if (checkpoint) {
            checkpoint();
        }
    };
    auto search = [&](double scale) {
        check();
        for (int i = 0;
             i <= int(std::ceil(
                      (settings.maximumOffset - settings.minimumOffset) / .1));
             ++i) {
            double offset = std::min(settings.maximumOffset,
                                     settings.minimumOffset + i * .1);
            coarse.push_back(score(scale, offset, stride));
        }
    };
    search(1);
    if (settings.mode != "offset") {
        const int steps = int(
            std::ceil((settings.maximumScale - settings.minimumScale) / .002));
        for (int i = 0; i <= steps; ++i) {
            search(std::min(settings.minimumScale + i * .002,
                            settings.maximumScale));
        }
    }
    auto seeds = peaks(coarse, duration, 8);
    auto offsetSeeds = coarse;
    offsetSeeds.erase(
        std::remove_if(offsetSeeds.begin(), offsetSeeds.end(),
                       [](const auto &c) { return c.scale != 1; }),
        offsetSeeds.end());
    auto offsets = peaks(offsetSeeds, duration, 3);
    seeds.insert(seeds.end(), offsets.begin(), offsets.end());
    std::vector<AlignmentCandidate> refined;
    for (const auto &seed : seeds) {
        check();
        AlignmentCandidate best = score(seed.scale, seed.offset);
        for (int si = -4; si <= 4; ++si) {
            const double scale = seed.scale + si * .0005;
            if ((settings.mode == "offset" && si != 0) ||
                scale < settings.minimumScale ||
                scale > settings.maximumScale) {
                continue;
            }
            for (int oi = -10; oi <= 10; ++oi) {
                const double offset = seed.offset + oi * .01;
                if (offset < settings.minimumOffset ||
                    offset > settings.maximumOffset) {
                    continue;
                }
                const auto c = score(scale, offset, stride);
                // Re-evaluate finalists at full resolution below.
                refined.push_back(c);
            }
        }
        refined.push_back(best);
    }
    auto finalists = peaks(refined, duration, 12);
    // Always retain a fully refined scale=1 solution for the auto simplicity
    // test.
    AlignmentCandidate fixed = score(1, offsets.front().offset);
    for (const auto &seed : offsets) {
        for (int i = -10; i <= 10; ++i) {
            double offset = seed.offset + i * .01;
            if (offset < settings.minimumOffset ||
                offset > settings.maximumOffset) {
                continue;
            }
            auto c = score(1, offset);
            if (better(c, fixed)) {
                fixed = c;
            }
        }
    }
    finalists.push_back(fixed);
    for (auto &c : finalists) {
        c = score(c.scale, c.offset);
    }
    auto candidates = peaks(finalists, duration, 3);
    if (settings.mode == "auto" &&
        candidates.front().score - fixed.score < .02) {
        candidates.insert(candidates.begin(), fixed);
        std::vector<AlignmentCandidate> kept{fixed};
        for (const auto &c : candidates) {
            if (kept.size() < 3 &&
                std::all_of(kept.begin(), kept.end(), [&](const auto &p) {
                    return distinct(c, p, duration);
                })) {
                kept.push_back(c);
            }
        }
        candidates = std::move(kept);
    }
    AlignmentResult result;
    result.settings = settings;
    result.candidates = candidates;
    const auto best = candidates.front();
    result.margin =
        candidates.size() > 1 ? best.score - candidates[1].score : 0;
    size_t consistent = 0;
    for (int i = 0; i < 6; ++i) {
        check();
        double begin = duration * i / 6, end = duration * (i + 1) / 6;
        auto local = score(best.scale, best.offset, 1, begin, end);
        double residual = 0;
        for (int j = -20; j <= 20; ++j) {
            auto c = score(best.scale, best.offset + j * .05, 1, begin, end);
            if (c.score > local.score) {
                local = c;
                residual = j * .05;
            }
        }
        result.windows.push_back({(begin + end) / 2, residual, local.score});
        consistent += std::abs(residual) <= .3 && local.score > .25;
    }
    const bool boundary = best.offset <= settings.minimumOffset + .02 ||
                          best.offset >= settings.maximumOffset - .02 ||
                          (settings.mode != "offset" &&
                           (best.scale <= settings.minimumScale + .0001 ||
                            best.scale >= settings.maximumScale - .0001));
    result.reliable = duration >= 10 && best.score > .35 &&
                      best.coverage > .75 && result.margin > .025 &&
                      consistent >= 4 && !boundary;
    double first = duration;
    for (const auto &f : m) {
        if (f.active) {
            first = f.time;
            break;
        }
    }
    result.firstMusicalTime = first;
    result.suggestedSilence = alignmentSilence(result, 0);
    result.diagnostic =
        result.reliable
            ? "Consistent global candidate; confidence is a heuristic, not a "
              "probability."
            : "Ambiguous or inconsistent alignment: inspect candidates and "
              "local residuals; no automatic application.";
    if (consistent < 6) {
        result.diagnostic += " Some sections have weak or displaced matches; "
                             "inspect the window diagnostics.";
    }
    if (boundary) {
        result.diagnostic += " Search boundary reached.";
    }
    if (result.suggestedSilence > 0) {
        result.diagnostic += " Notes precede audio zero: prepend the reported "
                             "silence manually and increase the offset by the "
                             "same amount. Audio was not modified.";
    }
    return result;
}

std::string alignmentJson(const AlignmentResult &result) {
    std::ostringstream out;
    {
        cereal::JSONOutputArchive archive(out);
        archive(cereal::make_nvp("alignment", result));
    }
    return out.str();
}

AlignmentResult readAlignment(const fs::path &path) {
    if (fs::file_size(path) > 1024 * 1024) {
        throw std::runtime_error("Alignment artifact too large");
    }
    std::ifstream input(path);
    return decodeAlignment(
        std::string(std::istreambuf_iterator<char>(input), {}));
}

double alignmentSilence(const AlignmentResult &result, size_t candidate) {
    const auto &c = result.candidates.at(candidate);
    return std::max(0.0, -(c.scale * result.firstMusicalTime + c.offset));
}

AlignmentResult decodeAlignment(const std::string &json) {
    if (json.size() > 1024 * 1024) {
        throw std::runtime_error("Alignment artifact too large");
    }
    std::istringstream input(json);
    AlignmentResult result;
    {
        cereal::JSONInputArchive archive(input);
        archive(cereal::make_nvp("alignment", result));
    }
    result.settings.validate();
    if (result.version != 1 || result.candidates.empty() ||
        result.candidates.size() > 3 ||
        !std::isfinite(result.firstMusicalTime) ||
        result.firstMusicalTime < 0 || result.windows.size() > 6) {
        throw std::runtime_error(
            "Invalid alignment artifact version/candidates");
    }
    for (const auto &c : result.candidates) {
        if (!std::isfinite(c.scale) || c.scale < .9 || c.scale > 1.1 ||
            !std::isfinite(c.offset) || std::abs(c.offset) > 120 ||
            !std::isfinite(c.score) || !std::isfinite(c.coverage)) {
            throw std::runtime_error("Invalid alignment candidate");
        }
    }
    return result;
}

void applyAlignment(Session &session, const AlignmentResult &result,
                    size_t candidate) {
    if (result.midiHash != session.state.inputHash) {
        throw std::runtime_error("Alignment belongs to another MIDI");
    }
    const auto &c = result.candidates.at(candidate);
    session.setAlignment(c.scale, c.offset, alignmentJson(result));
}
} // namespace ChartStudio
