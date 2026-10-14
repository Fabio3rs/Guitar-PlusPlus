#include "core.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace ChartStudio {
namespace {
int bits(int mask) {
    int count = 0;
    for (; mask; mask &= mask - 1) {
        ++count;
    }
    return count;
}
int center(int mask) {
    int total = 0;
    for (int lane = 0; lane < 5; ++lane) {
        if (mask & (1 << lane)) {
            total += lane * 12;
        }
    }
    return mask ? total / bits(mask) : 0;
}
int span(int mask) {
    int first = 5, last = 0;
    for (int lane = 0; lane < 5; ++lane) {
        if (mask & (1 << lane)) {
            first = std::min(first, lane);
            last = lane;
        }
    }
    return last - first;
}
int pitch(const Event &event) {
    int result = 0;
    for (const auto &note : event.sources) {
        result = std::max(result, note.pitch);
    }
    return result;
}
int laneLimit(const Settings &s) {
    return s.difficulty == "easy" ? 3 : s.difficulty == "medium" ? 4 : 5;
}
int chordLimit(const Settings &s) {
    return std::min(s.maxChord, s.difficulty == "easy"     ? 1
                                : s.difficulty == "expert" ? 3
                                                           : 2);
}
bool compatible(const Pattern &pattern, size_t count,
                const Settings &settings) {
    if (pattern.masks.size() != count) {
        return false;
    }
    for (int mask : pattern.masks) {
        if (mask < 0 || mask >= (1 << laneLimit(settings)) ||
            bits(mask) > chordLimit(settings)) {
            return false;
        }
    }
    return true;
}
struct Phrase {
    size_t begin = 0, count = 0;
    std::string exact, structural;
};
struct Candidate {
    int64_t cost = 0;
    std::vector<int> masks;
};
bool better(const Candidate &a, const Candidate &b) {
    return a.cost != b.cost ? a.cost < b.cost : a.masks < b.masks;
}
int64_t transition(int before, int after, int delta, const Settings &s) {
    const int movement = center(after) - center(before);
    int64_t cost = int64_t(s.weights[1]) * std::abs(movement);
    if ((delta > 0 && movement <= 0) || (delta < 0 && movement >= 0)) {
        cost += int64_t(s.weights[0]) * (24 + std::min(std::abs(delta), 24));
    }
    if (std::abs(movement) > 24) {
        cost += int64_t(s.weights[2]) * (std::abs(movement) - 24);
    }
    if (!delta && before != after) {
        cost += int64_t(s.weights[4]) * 24;
    }
    if (delta && before == after) {
        cost += int64_t(s.weights[5]) * 12;
    }
    return cost;
}
std::vector<Candidate> optimize(const std::vector<Event> &events,
                                const Phrase &phrase,
                                const Settings &settings) {
    std::vector<Candidate> beam(1);
    int low = 127, high = 0;
    for (size_t k = 0; k < phrase.count; ++k) {
        low = std::min(low, pitch(events[phrase.begin + k]));
        high = std::max(high, pitch(events[phrase.begin + k]));
    }
    for (size_t k = 0; k < phrase.count; ++k) {
        const auto &event = events[phrase.begin + k];
        std::set<int> pitches;
        for (const auto &note : event.sources) {
            pitches.insert(note.pitch);
        }
        const int count = std::min(int(pitches.size()), chordLimit(settings));
        std::vector<Candidate> next;
        for (const auto &candidate : beam) {
            for (int mask = 1; mask < (1 << laneLimit(settings)); ++mask) {
                if (bits(mask) != std::max(1, count)) {
                    continue;
                }
                Candidate n = candidate;
                // Phrase-relative contour reference, not a global pitch-to-lane
                // dictionary.
                const int expected =
                    high == low
                        ? 12
                        : (pitch(event) - low) *
                              ((laneLimit(settings) - 1) * 12) / (high - low);
                n.cost += int64_t(settings.weights[0]) *
                          std::abs(center(mask) - expected);
                n.cost += int64_t(settings.weights[3]) *
                          std::max(0, span(mask) - 2) * 12;
                if (k) {
                    n.cost += transition(
                        n.masks.back(), mask,
                        pitch(event) - pitch(events[phrase.begin + k - 1]),
                        settings);
                }
                n.masks.push_back(mask);
                next.push_back(std::move(n));
            }
        }
        std::sort(next.begin(), next.end(), better);
        const size_t width =
            settings.algorithm == "heuristic" ? 1 : size_t(settings.beam);
        if (next.size() > width) {
            next.resize(width);
        }
        beam = std::move(next);
    }
    if (beam.size() > 8) {
        beam.resize(8);
    }
    return beam;
}

std::vector<Phrase> segment(const std::vector<Event> &events,
                            const Score &score, const Settings &settings) {
    std::map<std::string, std::vector<size_t>> counts;
    // Bound candidate lengths and retain hashes only during discovery. Full
    // canonical equality is checked below.
    const size_t maximum = 32;
    std::vector<size_t> limit(events.size(), 1);
    for (size_t i = 0; i < events.size(); ++i) {
        for (size_t length = 1;
             length <= maximum && i + length <= events.size(); ++length) {
            if (length > 1) {
                const auto &previous = events[i + length - 2];
                if (events[i + length - 1].tick -
                        (previous.tick + previous.duration) >=
                    score.ppq) {
                    break;
                }
            }
            limit[i] = length;
            if (length >= 4) {
                counts[hash(canonical(events, i, length, score, settings,
                                      settings.structural))]
                    .push_back(i);
            }
        }
    }
    std::vector<Phrase> result;
    for (size_t i = 0; i < events.size();) {
        size_t length = 1;
        for (size_t n = limit[i]; n >= 4; --n) {
            const auto key =
                canonical(events, i, n, score, settings, settings.structural);
            bool repeated = false;
            if (counts[hash(key)].size() > 1) {
                // Hashes accelerate rejection only; compare the canonical
                // representation.
                for (size_t j : counts[hash(key)]) {
                    if ((j + n <= i || i + n <= j) && limit[j] >= n &&
                        hash(canonical(events, j, n, score, settings,
                                       settings.structural)) == hash(key) &&
                        canonical(events, j, n, score, settings,
                                  settings.structural) == key) {
                        repeated = true;
                        break;
                    }
                }
            }
            if (repeated) {
                length = n;
                break;
            }
        }
        if (length == 1) {
            // Unique passages remain musically useful optimization units, up to
            // one measure.
            while (length < limit[i] &&
                   events[i + length].tick - events[i].tick < 4LL * score.ppq &&
                   length < 8) {
                ++length;
            }
        }
        result.push_back({i, length,
                          canonical(events, i, length, score, settings, false),
                          canonical(events, i, length, score, settings, true)});
        i += length;
    }
    return result;
}
} // namespace

void generate(State &state, const std::vector<Pattern> &library,
              std::optional<std::pair<int64_t, int64_t>> region) {
    state.settings.validate();
    state.score.validate();
    const auto &settings = state.settings;
    if (settings.track < 0 ||
        size_t(settings.track) >= state.score.tracks.size()) {
        throw std::runtime_error(
            "Select a musical track explicitly before generating");
    }
    std::map<int64_t, Event> attacks;
    std::set<int> programs;
    for (const auto &source : state.score.tracks[settings.track].notes) {
        if ((settings.channel >= 0 && source.channel != settings.channel) ||
            (settings.program >= 0 && source.program != settings.program)) {
            continue;
        }
        if (source.channel == 9 && settings.channel != 9) {
            continue;
        }
        programs.insert(source.program / 8);
        auto &event = attacks[source.start];
        event.tick = source.start;
        event.sources.push_back(source);
    }
    if (attacks.empty()) {
        throw std::runtime_error("Selected source contains no musical attacks");
    }
    if (programs.size() > 1 && settings.program == -1) {
        throw std::runtime_error(
            "Track contains different instrument families; select a "
            "program/channel explicitly");
    }
    std::set<uint64_t> sourceIds;
    for (const auto &attack : attacks) {
        for (const auto &source : attack.second.sources) {
            sourceIds.insert(source.id);
        }
    }
    for (const auto &old : state.events) {
        for (const auto &source : old.sources) {
            if (!sourceIds.count(source.id)) {
                throw std::runtime_error(
                    "Source selection changed; reopen the MIDI in a new "
                    "project to preserve existing edits");
            }
        }
    }
    State next = state;
    next.conflicts.clear();
    std::vector<Event> events;
    for (auto &item : attacks) {
        auto event = std::move(item.second);
        event.duration = std::numeric_limits<int64_t>::max();
        for (const auto &source : event.sources) {
            event.duration =
                std::min(event.duration, source.end - source.start);
        }
        const auto old = std::find_if(
            state.events.begin(), state.events.end(), [&](const Event &e) {
                return !e.sources.empty() &&
                       e.sources.front().id == event.sources.front().id;
            });
        event.note.id =
            old == state.events.end() ? next.nextId++ : old->note.id;
        events.push_back(std::move(event));
    }
    auto phrases = segment(events, state.score, settings);
    std::map<std::string, std::vector<size_t>> groups;
    for (size_t p = 0; p < phrases.size(); ++p) {
        groups[settings.structural ? phrases[p].structural : phrases[p].exact]
            .push_back(p);
    }
    // Each group gets one decision. Group order is canonical, independent of
    // traversal and SQL row order.
    for (const auto &group : groups) {
        const auto &representative = phrases[group.second.front()];
        std::optional<Pattern> chosen;
        int priority = 100;
        std::string origin = "algorithm", reason = "new deterministic pattern";
        auto offer = [&](const Pattern &pattern, int rank,
                         const std::string &from, const std::string &why) {
            if (!compatible(pattern, representative.count, settings)) {
                if (pattern.status > 0) {
                    next.conflicts.push_back(
                        "Protected pattern incompatible with profile: " +
                        hash(pattern.exact));
                }
                return;
            }
            if (rank < priority ||
                (rank == priority && chosen && pattern.masks < chosen->masks)) {
                if (rank == priority && chosen &&
                    chosen->masks != pattern.masks) {
                    next.conflicts.push_back(
                        "Conflicting protected patterns: " +
                        hash(pattern.exact));
                }
                chosen = pattern;
                priority = rank;
                origin = from;
                reason = why;
            } else if (rank == priority && chosen &&
                       chosen->masks != pattern.masks) {
                next.conflicts.push_back("Conflicting protected patterns: " +
                                         hash(pattern.exact));
            }
        };
        for (size_t p : group.second) {
            const auto &phrase = phrases[p];
            auto it = state.patterns.find(phrase.exact);
            if (it != state.patterns.end() &&
                (it->second.status > 0 ||
                 (settings.reuse &&
                  it->second.parameters == settings.fingerprint()))) {
                offer(it->second,
                      it->second.status == 2   ? 0
                      : it->second.status == 1 ? 1
                                               : 3,
                      "project-cache", "exact");
            }
        }
        if (settings.reuse) {
            for (const auto &pattern : library) {
                bool exact = false;
                for (size_t p : group.second) {
                    exact |= pattern.exact == phrases[p].exact;
                }
                if (pattern.status > 0 &&
                    (exact ||
                     (settings.structural &&
                      pattern.structural == representative.structural))) {
                    offer(pattern, exact ? 4 : 5, "library-cache",
                          exact ? "exact" : "strict transposition");
                }
            }
            if (settings.structural) {
                for (const auto &entry : state.patterns) {
                    const auto &pattern = entry.second;
                    if (pattern.structural == representative.structural &&
                        (pattern.status > 0 ||
                         pattern.parameters == settings.fingerprint())) {
                        offer(pattern,
                              pattern.status == 2   ? 0
                              : pattern.status == 1 ? 1
                                                    : 5,
                              "project-cache", "strict transposition");
                    }
                }
            }
        }
        if (!chosen) {
            auto candidates = optimize(events, representative, settings);
            // Aggregate fixed-neighbor transitions over ALL occurrences before
            // selecting the shared pattern.
            for (auto &candidate : candidates) {
                for (size_t p : group.second) {
                    const auto &phrase = phrases[p];
                    if (phrase.begin && events[phrase.begin - 1].note.flags) {
                        candidate.cost +=
                            transition(events[phrase.begin - 1].note.flags,
                                       candidate.masks.front(),
                                       pitch(events[phrase.begin]) -
                                           pitch(events[phrase.begin - 1]),
                                       settings);
                    }
                    const size_t end = phrase.begin + phrase.count;
                    if (end < events.size() && events[end].note.flags) {
                        candidate.cost += transition(
                            candidate.masks.back(), events[end].note.flags,
                            pitch(events[end]) - pitch(events[end - 1]),
                            settings);
                    }
                }
            }
            std::sort(candidates.begin(), candidates.end(), better);
            Pattern pattern;
            pattern.exact = representative.exact;
            pattern.structural = representative.structural;
            pattern.parameters = settings.fingerprint();
            pattern.masks = candidates.front().masks;
            double retention = settings.difficulty == "easy"     ? .25
                               : settings.difficulty == "medium" ? .5
                               : settings.difficulty == "hard"   ? .75
                                                                 : 1;
            retention *= settings.density;
            for (size_t k = 1; k < pattern.masks.size(); ++k) {
                if (std::floor(static_cast<double>(k) * retention) ==
                    std::floor(static_cast<double>(k - 1) * retention)) {
                    pattern.masks[k] = 0;
                }
            }
            chosen = std::move(pattern);
        }
        for (size_t occurrence = 0; occurrence < group.second.size();
             ++occurrence) {
            const auto &phrase = phrases[group.second[occurrence]];
            for (size_t k = 0; k < phrase.count; ++k) {
                auto &event = events[phrase.begin + k];
                event.exact = phrase.exact;
                event.structural = phrase.structural;
                event.phrase = std::to_string(events[phrase.begin].note.id);
                event.note.flags = chosen->masks[k];
                event.removed = event.note.flags == 0;
                event.status = chosen->status;
                event.origin = origin == "algorithm" && occurrence
                                   ? "project-repeat"
                                   : origin;
                event.reason = reason;
            }
            Pattern stored = *chosen;
            stored.exact = phrase.exact;
            stored.structural = phrase.structural;
            const auto previous = next.patterns.find(stored.exact);
            if (previous == next.patterns.end() ||
                previous->second.status == 0) {
                next.patterns[stored.exact] = std::move(stored);
            }
        }
    }
    // Refine newly calculated group patterns against both boundaries in all
    // occurrences. Known decisions never participate in this optimization.
    // Three bounded coordinate passes avoid a greedy first-occurrence
    // transition choice without unbounded search.
    if (settings.density == 1 && settings.difficulty == "expert") {
        for (int pass = 0; pass < 3; ++pass) {
            bool changed = false;
            for (const auto &group : groups) {
                const auto &representative = phrases[group.second.front()];
                bool fixed = false;
                for (size_t p : group.second) {
                    for (size_t k = 0; k < phrases[p].count; ++k) {
                        const auto &e = events[phrases[p].begin + k];
                        fixed |= e.origin != "algorithm" &&
                                 e.origin != "project-repeat";
                        for (const auto &old : state.events) {
                            if (old.note.id == e.note.id &&
                                (old.manual || old.status > 0)) {
                                fixed = true;
                            }
                        }
                    }
                }
                if (fixed) {
                    continue;
                }
                auto candidates = optimize(events, representative, settings);
                for (auto &candidate : candidates) {
                    candidate.cost *= int64_t(group.second.size());
                    for (size_t p : group.second) {
                        const auto &phrase = phrases[p];
                        if (phrase.begin) {
                            candidate.cost +=
                                transition(events[phrase.begin - 1].note.flags,
                                           candidate.masks.front(),
                                           pitch(events[phrase.begin]) -
                                               pitch(events[phrase.begin - 1]),
                                           settings);
                        }
                        const size_t end = phrase.begin + phrase.count;
                        if (end < events.size()) {
                            candidate.cost += transition(
                                candidate.masks.back(), events[end].note.flags,
                                pitch(events[end]) - pitch(events[end - 1]),
                                settings);
                        }
                    }
                }
                std::sort(candidates.begin(), candidates.end(), better);
                const auto &best = candidates.front();
                for (size_t p : group.second) {
                    const auto &phrase = phrases[p];
                    for (size_t k = 0; k < phrase.count; ++k) {
                        auto &e = events[phrase.begin + k];
                        changed |= e.note.flags != best.masks[k];
                        e.note.flags = best.masks[k];
                        e.cost = best.cost;
                    }
                    next.patterns[phrase.exact].masks = best.masks;
                }
            }
            if (!changed) {
                break;
            }
        }
    }
    // Manual edits, locked data and material outside a selected region are
    // authoritative.
    for (const auto &old : state.events) {
        const auto it =
            std::find_if(events.begin(), events.end(), [&](const Event &e) {
                return e.note.id == old.note.id;
            });
        const bool outside =
            region && (old.tick < region->first || old.tick >= region->second);
        if (old.manual || old.status > 0 || outside || old.sources.empty()) {
            if (it == events.end()) {
                events.push_back(old);
            } else {
                if (old.status > 0 && (it->note.flags != old.note.flags ||
                                       it->removed != old.removed)) {
                    next.conflicts.push_back("Preserved protected occurrence " +
                                             std::to_string(old.note.id));
                }
                *it = old;
            }
        }
    }
    if (region) {
        events.erase(std::remove_if(events.begin(), events.end(),
                                    [&](const Event &e) {
                                        return (e.tick < region->first ||
                                                e.tick >= region->second) &&
                                               std::none_of(
                                                   state.events.begin(),
                                                   state.events.end(),
                                                   [&](const Event &old) {
                                                       return old.note.id ==
                                                              e.note.id;
                                                   });
                                    }),
                     events.end());
    }
    next.events = std::move(events);
    synchronize(next);
    if (next.original.empty()) {
        next.original = next.events;
    }
    validate(next);
    state = std::move(next);
}
} // namespace ChartStudio
