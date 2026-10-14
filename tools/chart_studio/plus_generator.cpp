#include "plus_generator.h"
#include "plus_simulation.h"
#include <algorithm>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <tuple>

namespace ChartStudio {
namespace {
int gems(int mask) {
    int result = 0;
    for (; mask != 0; mask &= mask - 1) {
        ++result;
    }
    return result;
}
size_t atTick(const std::vector<PlusAttack> &a, int64_t tick) {
    return size_t(std::lower_bound(a.begin(), a.end(), tick,
                                   [](const auto &note, int64_t t) {
                                       return note.tick < t;
                                   }) -
                  a.begin());
}
bool order(const PlusPhrase &a, const PlusPhrase &b) {
    return std::tie(a.begin, a.end, a.id) < std::tie(b.begin, b.end, b.id);
}
bool compatible(const PlusAnalysis &a, const PlusPhrase &left,
                const PlusPhrase &right) {
    const auto l = plusAttackRange(a.attacks, left);
    const auto r = plusAttackRange(a.attacks, right);
    // The existing loader consumes an outside attack when advancing its phrase
    // cursor.
    return left.end <= right.begin && l.second < r.first;
}
bool fits(const PlusAnalysis &a, const std::vector<PlusPhrase> &chosen,
          const PlusPhrase &p) {
    for (const auto &other : chosen) {
        if (!(other.begin < p.begin ? compatible(a, other, p)
                                    : compatible(a, p, other))) {
            return false;
        }
    }
    return true;
}
double phraseRisk(const PlusAnalysis &a, size_t first, size_t end) {
    double chords = 0;
    double movement = 0;
    for (size_t i = first; i < end; ++i) {
        chords += static_cast<double>(gems(a.attacks[i].mask) > 1);
        if (i > first) {
            const auto center = [](int mask) {
                double sum = 0;
                double count = 0;
                for (int lane = 0; lane < 5; ++lane) {
                    if ((mask & (1 << lane)) != 0) {
                        sum += lane;
                        ++count;
                    }
                }
                return count > 0 ? sum / count : 2.0;
            };
            movement += std::abs(center(a.attacks[i].mask) -
                                 center(a.attacks[i - 1].mask));
        }
    }
    const auto count = double(end - first);
    const double duration =
        std::max(.25, a.attacks[end - 1].time - a.attacks[first].time);
    return std::clamp((.25 * count / 24) + (.35 * (count / duration) / 8) +
                          (.2 * chords / count) + (.2 * movement / (4 * count)),
                      0.0, 1.0);
}
} // namespace

void PlusSettings::validate() const {
    if ((profile != "balanced" && profile != "strategic" &&
         profile != "generous" && profile != "sparse") ||
        !std::isfinite(reserve) || reserve < 0 || reserve > 1 ||
        !std::isfinite(spacingBars) || spacingBars < 1 || spacingBars > 128 ||
        (preferredBars != 1 && preferredBars != 2)) {
        throw std::runtime_error(
            "Invalid Plus profile, reserve, phrase length or spacing");
    }
    for (double weight : {structureWeight, strategyWeight, riskWeight}) {
        if (!std::isfinite(weight) || weight < 0 || weight > 100) {
            throw std::runtime_error(
                "Plus weights must be finite and between 0 and 100");
        }
    }
}
PlusSettings plusProfile(const std::string &name) {
    PlusSettings result;
    result.enabled = true;
    result.profile = name;
    if (name == "balanced") {
        result.strategyWeight = 2;
    }
    if (name == "generous") {
        result.spacingBars = 6;
        result.riskWeight = 1;
    }
    if (name == "sparse") {
        result.spacingBars = 12;
        result.preferredBars = 2;
    }
    result.validate();
    return result;
}
std::pair<size_t, size_t> plusAttackRange(const std::vector<PlusAttack> &a,
                                          const PlusPhrase &p) {
    return {atTick(a, p.begin), atTick(a, p.end)};
}

double PlusAnalysis::seconds(int64_t tick) const {
    Score score;
    score.ppq = ppq;
    score.tempos = tempos;
    return (scale * score.seconds(tick)) + offset;
}

std::vector<PlusAttack> plusAttacks(const State &state) {
    validate(state);
    std::vector<PlusAttack> attacks;
    std::map<int64_t, std::pair<int, int64_t>> grouped;
    for (const auto &e : state.events) {
        if (e.removed) {
            continue;
        }
        auto [it, added] = grouped.emplace(
            e.tick, std::make_pair(e.note.flags, exportDuration(state, e)));
        if (!added) {
            it->second.first |= e.note.flags;
            it->second.second =
                std::min(it->second.second, exportDuration(state, e));
        }
    }
    for (const auto &entry : grouped) {
        const auto tick = entry.first;
        const auto end = tick + entry.second.second;
        attacks.push_back({tick, end, audioSeconds(state, tick),
                           audioSeconds(state, end), entry.second.first});
    }
    return attacks;
}

PlusAnalysis analyzePlus(const State &state) {
    PlusAnalysis result;
    result.attacks = plusAttacks(state);
    result.ppq = state.score.ppq;
    result.tempos = state.score.tempos;
    result.scale = state.alignment.scale;
    result.offset = state.settings.offset;
    if (result.attacks.empty()) {
        result.diagnostics.push_back("No playable attacks");
        return result;
    }
    int64_t endTick = result.attacks.back().tick + 1;
    for (const auto &a : result.attacks) {
        endTick = std::max(endTick, a.endTick);
    }
    auto signatures = state.score.signatures;
    std::stable_sort(signatures.begin(), signatures.end(),
                     [](auto a, auto b) { return a.tick < b.tick; });
    if (signatures.empty() || signatures.front().tick > 0) {
        signatures.insert(signatures.begin(), Signature{});
        result.diagnostics.push_back("4/4 assumed before first time signature");
    }
    for (size_t i = 0; i < signatures.size(); ++i) {
        const auto &s = signatures[i];
        const int64_t stop = i + 1 < signatures.size()
                                 ? std::min(endTick, signatures[i + 1].tick)
                                 : endTick;
        const int64_t numerator = int64_t(state.score.ppq) * 4 * s.numerator;
        for (int64_t bar = 0;; ++bar) {
            const int64_t tick =
                s.tick +
                ((bar * numerator + s.denominator / 2) / s.denominator);
            if (tick >= stop) {
                break;
            }
            if (result.bars.size() >= 100000) {
                throw std::runtime_error(
                    "Plus analysis exceeds 100000 measures");
            }
            if (result.bars.empty() || result.bars.back() != tick) {
                result.bars.push_back(tick);
            }
        }
        if (i + 1 < signatures.size() && stop == signatures[i + 1].tick &&
            ((stop - s.tick) * s.denominator) % numerator != 0) {
            result.diagnostics.push_back(
                "Time signature change truncates a measure");
        }
    }
    result.bars.push_back(endTick);
    // Prefix attack values, and a union of sustained spans: chords do not
    // multiply score.
    std::vector<double> values;
    for (size_t i = 0; i + 1 < result.bars.size(); ++i) {
        const double begin = audioSeconds(state, result.bars[i]);
        double value = 0;
        for (double window : {4., 8., 12., 20.}) {
            double subtotal = 0;
            double heldUntil = begin;
            for (size_t j = 0; j < result.attacks.size() &&
                               result.attacks[j].time < begin + window;
                 ++j) {
                const auto &note = result.attacks[j];
                if (note.time >= begin) {
                    subtotal += 50;
                }
                const double sustainEnd =
                    std::min(begin + window, note.endTime - .13);
                subtotal +=
                    50 *
                    std::max(0.0, sustainEnd -
                                      std::max({note.time, heldUntil, begin}));
                heldUntil = std::max(heldUntil, sustainEnd);
            }
            value += subtotal / window / 4;
        }
        values.push_back(value);
    }
    auto sorted = values;
    std::sort(sorted.begin(), sorted.end());
    const double threshold = sorted.empty() ? 0 : sorted[sorted.size() * 2 / 3];
    std::vector<size_t> peaks(values.size());
    std::iota(peaks.begin(), peaks.end(), 0);
    std::stable_sort(peaks.begin(), peaks.end(),
                     [&](size_t a, size_t b) { return values[a] > values[b]; });
    for (size_t i : peaks) {
        const double time = audioSeconds(state, result.bars[i]);
        if (values[i] <= 0 || values[i] < threshold ||
            atTick(result.attacks, result.bars[i]) == result.attacks.size()) {
            continue;
        }
        if (std::any_of(result.regions.begin(), result.regions.end(),
                        [&](auto r) { return std::abs(r.time - time) < 20; })) {
            continue;
        }
        const auto last = std::min(
            endTick, state.score.ticks((time + 8 - state.settings.offset) /
                                       state.alignment.scale));
        result.regions.push_back(
            {result.bars[i], last, time, audioSeconds(state, last), values[i]});
    }
    // Only explicit solo markers; section names are not guessed to be solos.
    for (const auto &marker : state.score.markers) {
        if (marker.text != "solo" && marker.text != "solo_start" &&
            marker.text != "[solo_on]") {
            continue;
        }
        const double time = audioSeconds(state, marker.tick);
        if (atTick(result.attacks, marker.tick) == result.attacks.size()) {
            continue;
        }
        if (std::any_of(result.regions.begin(), result.regions.end(),
                        [&](auto r) { return std::abs(r.time - time) < 8; })) {
            continue;
        }
        const auto last = std::min(
            endTick, state.score.ticks((time + 8 - state.settings.offset) /
                                       state.alignment.scale));
        result.regions.push_back(
            {marker.tick, last, time, audioSeconds(state, last), 0});
    }
    std::sort(result.regions.begin(), result.regions.end(),
              [](auto a, auto b) { return a.begin < b.begin; });
    return result;
}

std::vector<std::string> plusProblems(const State &state) {
    PlusAnalysis a;
    a.attacks = plusAttacks(state);
    auto phrases = state.plus.phrases;
    std::sort(phrases.begin(), phrases.end(), order);
    std::vector<std::string> errors;
    for (size_t i = 0; i < phrases.size(); ++i) {
        const auto &p = phrases[i];
        const auto [first, end] = plusAttackRange(a.attacks, p);
        const auto label = "Plus #" + std::to_string(p.id) + ": ";
        if (first == end) {
            errors.push_back(label + "no playable attacks");
        } else if (audioSeconds(state, p.end) - a.attacks[end - 1].time <
                   .075) {
            errors.push_back(
                label + "last attack too close to phrase end (needs 75 ms)");
        }
        if ((i != 0u) && !compatible(a, phrases[i - 1], p)) {
            errors.push_back(
                label + "overlap or missing outside attack between phrases");
        }
    }
    return errors;
}

PlusProposal proposePlus(const State &state, const PlusSettings &settings,
                         std::optional<std::pair<int64_t, int64_t>> region,
                         const std::function<void()> &checkpoint) {
    settings.validate();
    if (region && (region->first < 0 || region->second <= region->first)) {
        throw std::runtime_error("Invalid Plus generation region");
    }
    PlusProposal out;
    out.source = hash(encode(state));
    out.document = state.plus;
    out.document.settings = settings;
    out.analysis = analyzePlus(state);
    const auto &a = out.analysis;
    auto &chosen = out.document.phrases;
    chosen.erase(std::remove_if(chosen.begin(), chosen.end(),
                                [&](const auto &p) {
                                    return !p.manual && p.status == 0 &&
                                           (!region ||
                                            (p.begin >= region->first &&
                                             p.end <= region->second));
                                }),
                 chosen.end());
    if (a.attacks.empty()) {
        out.diagnostics = a.diagnostics;
        return out;
    }
    std::vector<PlusPhrase> candidates;
    for (size_t i = 0; i + 1 < a.bars.size(); ++i) {
        for (size_t bars : {size_t(1), size_t(2)}) {
            if (i + bars >= a.bars.size()) {
                continue;
            }
            PlusPhrase p;
            p.begin = a.bars[i];
            p.end = a.bars[i + bars];
            if (region && (p.begin < region->first || p.end > region->second)) {
                continue;
            }
            const auto [first, end] = plusAttackRange(a.attacks, p);
            if (end - first < 2 ||
                audioSeconds(state, p.end) - a.attacks[end - 1].time < .075) {
                continue;
            }
            bool gap = false;
            const double barSeconds = audioSeconds(state, a.bars[i + 1]) -
                                      audioSeconds(state, p.begin);
            for (size_t n = first + 1; n < end; ++n) {
                gap |= a.attacks[n].time - std::max(a.attacks[n - 1].time,
                                                    a.attacks[n - 1].endTime) >
                       std::max(2.0, barSeconds);
            }
            if (gap) {
                continue;
            }
            p.risk = phraseRisk(a, first, end);
            p.reason = "Measure boundaries; " + std::to_string(end - first) +
                       " attacks; " + std::to_string(bars) + " measure(s)";
            candidates.push_back(p);
        }
    }
    // Existing mapped motives can supply boundaries between bar lines.
    for (size_t i = 0; i < state.events.size();) {
        size_t end = i + 1;
        while (end < state.events.size() &&
               state.events[end].phrase == state.events[i].phrase) {
            ++end;
        }
        if (!state.events[i].phrase.empty() && end < state.events.size()) {
            PlusPhrase p;
            p.begin = state.events[i].tick;
            p.end = state.events[end].tick;
            const auto range = plusAttackRange(a.attacks, p);
            const double beats = double(p.end - p.begin) / state.score.ppq;
            if ((!region ||
                 (p.begin >= region->first && p.end <= region->second)) &&
                range.second - range.first >= 2 && beats >= 2 && beats <= 8 &&
                audioSeconds(state, p.end) - a.attacks[range.second - 1].time >=
                    .075) {
                bool gap = false;
                for (size_t n = range.first + 1; n < range.second; ++n) {
                    gap |=
                        a.attacks[n].time - std::max(a.attacks[n - 1].time,
                                                     a.attacks[n - 1].endTime) >
                        2;
                }
                if (!gap) {
                    p.risk = phraseRisk(a, range.first, range.second);
                    p.reason = "Recognized musical motif; " +
                               std::to_string(range.second - range.first) +
                               " attacks";
                    candidates.push_back(p);
                }
            }
        }
        i = end;
    }
    std::stable_sort(candidates.begin(), candidates.end(), order);
    candidates.erase(std::unique(candidates.begin(), candidates.end(),
                                 [](const auto &p, const auto &q) {
                                     return p.begin == q.begin &&
                                            p.end == q.end;
                                 }),
                     candidates.end());
    // Backward preparation with a real carried balance under the dense
    // activation policy.
    PlusSimulationSettings sim;
    sim.reserve = std::max(.01, settings.reserve);
    const auto scoreCandidate = [&](const PlusPhrase &p, double targetTime) {
        const auto firstBar =
            std::lower_bound(a.bars.begin(), a.bars.end(), p.begin);
        const auto lastBar =
            std::lower_bound(a.bars.begin(), a.bars.end(), p.end);
        const auto length = double(lastBar - firstBar);
        return (settings.structureWeight /
                (1 + std::abs(length - settings.preferredBars))) -
               (settings.riskWeight * p.risk) -
               (.01 * std::max(0., targetTime - audioSeconds(state, p.end)));
    };
    const auto add = [&](PlusPhrase p, const std::string &reason) {
        const auto old =
            std::find_if(state.plus.phrases.begin(), state.plus.phrases.end(),
                         [&](const auto &q) {
                             return q.begin == p.begin && q.end == p.end;
                         });
        p.id =
            old == state.plus.phrases.end() ? out.document.nextId++ : old->id;
        p.reason += "; " + reason;
        chosen.push_back(std::move(p));
        std::sort(chosen.begin(), chosen.end(), order);
    };
    for (size_t target = 0; target < a.regions.size(); ++target) {
        if (checkpoint) {
            checkpoint();
        }
        for (int attempt = 0; attempt < 5; ++attempt) {
            const auto simulation = simulatePlus(a, chosen, sim);
            if (simulation.regionEnergy[target] + 1e-9 >= settings.reserve) {
                break;
            }
            const auto &r = a.regions[target];
            const PlusPhrase *best = nullptr;
            double bestScore = -std::numeric_limits<double>::infinity();
            for (const auto &p : candidates) {
                if (audioSeconds(state, p.end) > r.time ||
                    !fits(a, chosen, p)) {
                    continue;
                }
                const auto distance =
                    std::lower_bound(a.bars.begin(), a.bars.end(), r.begin) -
                    std::lower_bound(a.bars.begin(), a.bars.end(), p.begin);
                if (distance > 32) {
                    continue;
                }
                auto trial = chosen;
                trial.push_back(p);
                const auto benefit =
                    simulatePlus(a, trial, sim).regionEnergy[target] -
                    simulation.regionEnergy[target];
                if (benefit < 1e-9) {
                    continue;
                }
                const double value =
                    std::round((scoreCandidate(p, r.time) +
                                settings.strategyWeight * benefit * 10) *
                               1e6) /
                    1e6;
                if (value > bestScore) {
                    best = &p;
                    bestScore = value;
                }
            }
            if (best == nullptr) {
                break;
            }
            add(*best,
                "prepares reserve before region " + std::to_string(target + 1));
        }
    }
    // Fill musically useful gaps, with saturation measured rather than a
    // periodic S2 grid.
    for (size_t block = 0; block + 1 < a.bars.size();
         block += size_t(std::max(1., settings.spacingBars))) {
        if (checkpoint) {
            checkpoint();
        }
        const auto stop = a.bars[std::min(
            a.bars.size() - 1, block + size_t(settings.spacingBars))];
        if (std::any_of(chosen.begin(), chosen.end(), [&](const auto &p) {
                return p.begin >= a.bars[block] && p.begin < stop;
            })) {
            continue;
        }
        const PlusPhrase *best = nullptr;
        double value = -std::numeric_limits<double>::infinity();
        const auto baseline = simulatePlus(a, chosen, sim);
        for (const auto &p : candidates) {
            if (p.begin < a.bars[block] || p.begin >= stop ||
                !fits(a, chosen, p)) {
                continue;
            }
            if (a.regions.empty() || p.end >= a.regions.back().end) {
                continue;
            }
            auto trial = chosen;
            trial.push_back(p);
            const auto result = simulatePlus(a, trial, sim);
            if (result.wasted > baseline.wasted + .1) {
                continue;
            }
            const double merit = scoreCandidate(p, audioSeconds(state, p.end)) +
                                 ((result.benefit - baseline.benefit) / 500);
            if (merit > value) {
                best = &p;
                value = merit;
            }
        }
        if (best != nullptr) {
            add(*best, "fills opportunity gap with limited saturation");
        }
    }
    // Bounded discrete refinement. The complete carried-energy schedule is
    // evaluated for each alternative, not an independent per-phrase score.
    const auto utility = [&](const std::vector<PlusPhrase> &phrases) {
        const auto result = simulatePlus(a, phrases, sim);
        double score = (result.benefit / 2000) - (result.wasted * 4);
        for (double energy : result.regionEnergy) {
            score += settings.strategyWeight *
                     std::min(1., energy / std::max(.01, settings.reserve));
        }
        for (const auto &p : phrases) {
            score -= settings.riskWeight * p.risk * .2;
        }
        const double targetCount =
            double(a.bars.size() - 1) / settings.spacingBars;
        score -= .15 * std::abs(double(phrases.size()) - targetCount);
        return std::llround(score * 1000000);
    };
    for (int pass = 0; pass < 4; ++pass) {
        bool improved = false;
        int evaluations = 0;
        auto current = utility(chosen);
        for (size_t i = 0; i < chosen.size() && evaluations < 64; ++i) {
            const auto previous = chosen[i];
            if (previous.manual || previous.status != 0 ||
                (region && (previous.begin < region->first ||
                            previous.end > region->second))) {
                continue;
            }
            auto reduced = chosen;
            reduced.erase(reduced.begin() + static_cast<std::ptrdiff_t>(i));
            ++evaluations;
            const auto removedScore = utility(reduced);
            if (removedScore > current) {
                chosen = std::move(reduced);
                improved = true;
                break;
            }
            for (const auto &candidate : candidates) {
                if (evaluations >= 64) {
                    break;
                }
                if (candidate.begin == previous.begin &&
                    candidate.end == previous.end) {
                    continue;
                }
                if (std::abs(audioSeconds(state, candidate.begin) -
                             audioSeconds(state, previous.begin)) > 8 ||
                    !fits(a, reduced, candidate)) {
                    continue;
                }
                if (checkpoint) {
                    checkpoint();
                }
                auto trial = reduced;
                trial.push_back(candidate);
                ++evaluations;
                const auto alternative = utility(trial);
                if (alternative > current) {
                    auto replacement = candidate;
                    replacement.id = previous.id;
                    replacement.reason += "; global carried-energy refinement";
                    chosen[i] = replacement;
                    std::sort(chosen.begin(), chosen.end(), order);
                    improved = true;
                    break;
                }
            }
            if (improved) {
                break;
            }
        }
        if (!improved) {
            break;
        }
    }
    const auto final = simulatePlus(a, chosen, sim);
    for (size_t i = 0; i < a.regions.size(); ++i) {
        if (final.regionEnergy[i] + 1e-9 < settings.reserve) {
            out.diagnostics.push_back(
                "Region " + std::to_string(i + 1) +
                ": desired reserve unavailable; no notes invented");
        }
    }
    State check = state;
    check.plus = out.document;
    const auto errors = plusProblems(check);
    out.diagnostics.insert(out.diagnostics.end(), errors.begin(), errors.end());
    return out;
}
} // namespace ChartStudio
