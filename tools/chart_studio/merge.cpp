#include "merge.h"
#include "plus_generator.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ChartStudio {
namespace {
struct Fraction {
    int64_t n = 0, d = 1;
};
Fraction fraction(const std::string &text) {
    const auto slash = text.find('/');
    auto integer = [](const std::string &s) -> int64_t {
        if (s.empty() ||
            s.find_first_not_of("0123456789") != std::string::npos) {
            throw std::runtime_error(
                "Musical duration must be a nonnegative integer or fraction");
        }
        const auto value = std::stoll(s);
        if (value > 1024) {
            throw std::runtime_error(
                "Musical duration numerator/denominator exceeds 1024");
        }
        return value;
    };
    Fraction f{integer(text.substr(0, slash)),
               slash == std::string::npos ? 1
                                          : integer(text.substr(slash + 1))};
    if (!f.d) {
        throw std::runtime_error("Musical duration denominator is zero");
    }
    return f;
}
bool atLeast(int64_t ticks, int ppq, Fraction f) {
    return ticks * f.d >= 4LL * ppq * f.n;
}
bool atMost(int64_t ticks, int ppq, Fraction f) {
    return ticks * f.d <= 4LL * ppq * f.n;
}
bool protectedEvent(const Event &e) { return e.manual || e.status > 0; }
State sourceState(const State &state, const Part &part) {
    State result = partState(state, state.partId);
    result.merges.clear();
    static_cast<Part &>(result) = part;
    result.settings.offset = state.settings.offset;
    synchronize(result);
    return result;
}
std::vector<MergeRegion> schedule(const State &primary,
                                  const MergeConfig &config) {
    int64_t end = 1;
    for (const auto &track : primary.score.tracks) {
        for (const auto &note : track.notes) {
            end = std::max(end, note.end + 1);
        }
    }
    for (const auto &e : primary.events) {
        end = std::max(end, e.tick + e.duration + 1);
    }
    for (const auto &r : config.regions) {
        end = std::max(end, r.end);
    }
    const auto minimum = fraction(config.minimum);
    const auto clearance = fraction(config.clearance);
    const int64_t margin =
        (4LL * primary.score.ppq * clearance.n + clearance.d - 1) / clearance.d;
    std::vector<MergeRegion> secondary;
    auto gap = [&](int64_t a, int64_t b) {
        if (b <= a || !atLeast(b - a, primary.score.ppq, minimum) ||
            (config.maximum != "off" &&
             !atMost(b - a, primary.score.ppq, fraction(config.maximum)))) {
            return;
        }
        if (b - a > 2 * margin + (a > 0 ? 1 : 0)) {
            secondary.push_back({a + margin + (a > 0 ? 1 : 0), b - margin, 1});
        }
    };
    int64_t occupiedEnd = 0;
    for (const auto &e : primary.events) {
        if (e.removed) {
            continue;
        }
        gap(occupiedEnd, e.tick);
        occupiedEnd =
            std::max(occupiedEnd,
                     e.tick + std::max<int64_t>(1, exportDuration(primary, e)));
    }
    gap(occupiedEnd, end);
    std::vector<int64_t> boundaries{0, end};
    for (const auto &r : secondary) {
        boundaries.push_back(r.begin);
        boundaries.push_back(r.end);
    }
    for (const auto &r : config.regions) {
        boundaries.push_back(r.begin);
        boundaries.push_back(r.end);
    }
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()),
                     boundaries.end());
    std::vector<MergeRegion> result;
    for (size_t i = 1; i < boundaries.size(); ++i) {
        const auto begin = boundaries[i - 1], finish = boundaries[i];
        int source = 0;
        for (const auto &r : secondary) {
            if (begin >= r.begin && begin < r.end) {
                source = 1;
                break;
            }
        }
        for (const auto &r : config.regions) {
            if (begin >= r.begin && begin < r.end) {
                source = r.source;
                break;
            }
        }
        if (!result.empty() && result.back().source == source) {
            result.back().end = finish;
        } else {
            result.push_back({begin, finish, source});
        }
    }
    return result;
}
} // namespace

int64_t mergeSnap(int64_t tick, int ppq, const std::string &division) {
    const auto f = fraction(division);
    if (!f.n || tick < 0 || tick > INT32_MAX || ppq <= 0 || ppq > 32767) {
        throw std::runtime_error("Invalid musical grid/tick");
    }
    const auto unit = 4LL * ppq * f.n;
    const auto scaled = tick * f.d;
    auto index = scaled / unit;
    if ((scaled % unit) * 2 > unit) {
        ++index;
    }
    const auto value = index * unit;
    auto result = value / f.d;
    if ((value % f.d) * 2 > f.d) {
        ++result;
    }
    return std::min<int64_t>(INT32_MAX, result);
}

void validateMerge(const State &state, const MergeConfig &config) {
    auto secondary = state.settings;
    secondary.track = config.secondaryTrack;
    secondary.channel = config.secondaryChannel;
    secondary.program = config.secondaryProgram;
    secondary.validate();
    if (secondary.track < 0 ||
        size_t(secondary.track) >= state.score.tracks.size() ||
        state.settings.track < 0 ||
        size_t(state.settings.track) >= state.score.tracks.size()) {
        throw std::runtime_error(
            "Choose valid primary and secondary MIDI tracks");
    }
    if (secondary.track == state.settings.track &&
        secondary.channel == state.settings.channel &&
        secondary.program == state.settings.program) {
        throw std::runtime_error("Primary and secondary source must differ");
    }
    const auto minimum = fraction(config.minimum);
    if (config.maximum != "off") {
        const auto maximum = fraction(config.maximum);
        if (maximum.n * minimum.d < minimum.n * maximum.d) {
            throw std::runtime_error(
                "Maximum silence is below minimum silence");
        }
    }
    (void)fraction(config.clearance);
    (void)mergeSnap(0, state.score.ppq, config.grid);
    auto regions = config.regions;
    std::sort(regions.begin(), regions.end(),
              [](const auto &a, const auto &b) { return a.begin < b.begin; });
    int64_t end = 0;
    for (const auto &r : regions) {
        if (r.begin < end || r.end <= r.begin || r.end > INT32_MAX ||
            r.source < 0 || r.source > 2) {
            throw std::runtime_error("Manual merge regions must be "
                                     "nonoverlapping [begin,end) intervals");
        }
        end = r.end;
    }
}

MergeProposal proposeMerge(const State &input, const MergeConfig &config,
                           const std::vector<Pattern> &library,
                           bool regenerateSources) {
    validateMerge(input, config);
    MergeProposal proposal{hash(encode(input)), input};
    auto &state = proposal.state;
    auto &doc = state.merges[state.partId];
    if (doc.initialized &&
        (doc.primary.settings.track != state.settings.track ||
         doc.primary.settings.channel != state.settings.channel ||
         doc.primary.settings.program != state.settings.program ||
         doc.secondary.settings.track != config.secondaryTrack ||
         doc.secondary.settings.channel != config.secondaryChannel ||
         doc.secondary.settings.program != config.secondaryProgram)) {
        throw std::runtime_error("Merge sources changed: detach the "
                                 "composition before selecting new sources");
    }
    if (!doc.initialized) {
        doc.primary = static_cast<const Part &>(input);
        doc.secondary = Part{};
        doc.secondary.partId = state.partId;
    }
    doc.config = config;
    auto a = sourceState(state, doc.primary),
         b = sourceState(state, doc.secondary);
    a.settings = state.settings;
    b.settings = state.settings;
    b.settings.track = config.secondaryTrack;
    b.settings.channel = config.secondaryChannel;
    b.settings.program = config.secondaryProgram;
    // Existing mapped sources are reused by default. UMAP needs its original
    // artifact to map a new source; never silently fall back to beam.
    auto mapSource = [&](State &mapped) {
        if (!regenerateSources && !mapped.events.empty()) {
            return;
        }
        const auto &notes =
            mapped.score.tracks.at(size_t(mapped.settings.track)).notes;
        const bool any =
            std::any_of(notes.begin(), notes.end(), [&](const auto &note) {
                return (mapped.settings.channel < 0 ||
                        mapped.settings.channel == note.channel) &&
                       (mapped.settings.program < 0 ||
                        mapped.settings.program == note.program) &&
                       (note.channel != 9 || mapped.settings.channel == 9);
            });
        if (any) {
            generate(mapped, library);
        } else if (!mapped.events.empty()) {
            throw std::runtime_error("Mapped source no longer contains "
                                     "attacks; explicitly reset the source");
        }
    };
    mapSource(a);
    mapSource(b);
    synchronize(a);
    synchronize(b);
    if (!doc.initialized) {
        state.nextId = std::max(state.nextId, a.nextId);
    }
    doc.primary = static_cast<const Part &>(a);
    doc.secondary = static_cast<const Part &>(b);
    doc.regions = schedule(a, config);
    doc.decisions.clear();
    std::vector<Event> result;
    for (int source = 0; source < 2; ++source) {
        const auto &mapped = source == 0 ? a : b;
        for (const auto &original : mapped.events) {
            if (original.removed) {
                continue;
            }
            const auto region = std::find_if(
                doc.regions.begin(), doc.regions.end(), [&](const auto &r) {
                    return original.tick >= r.begin && original.tick < r.end;
                });
            MergeDecision decision;
            decision.source = source;
            decision.sourceId = original.note.id;
            decision.tick = original.tick;
            decision.originalDuration = original.duration;
            decision.playableDuration = exportDuration(mapped, original);
            if (region == doc.regions.end() || region->source != source) {
                decision.reason = "Excluded by source selection";
                doc.decisions.push_back(decision);
                continue;
            }
            auto event = original;
            const auto key =
                std::to_string(source) + ":" + std::to_string(original.note.id);
            auto id = doc.ids.find(key);
            if (id == doc.ids.end()) {
                const auto value = source == 0 && !doc.initialized
                                       ? original.note.id
                                       : state.nextId++;
                id = doc.ids.emplace(key, value).first;
            }
            event.note.id = id->second;
            const auto duration = decision.playableDuration;
            event.duration =
                source == 0 && duration <= region->end - event.tick
                    ? duration
                    : std::min(duration, std::max<int64_t>(
                                             0, region->end - event.tick - 1));
            // Keep traceability, but do not offer a clipped motif for global
            // promotion under the original canonical signature.
            event.exact.clear();
            event.structural.clear();
            event.phrase =
                "merge-" + std::to_string(source) + "-" + original.phrase;
            event.reason = "Merge source " + std::to_string(source);
            event.origin = "merge";
            decision.id = event.note.id;
            decision.duration = event.duration;
            decision.copied = true;
            decision.reason =
                event.duration < duration
                    ? "Copied; sustain truncated by source transition"
                : duration < original.duration
                    ? "Copied; existing source sustain policy"
                    : "Copied";
            result.push_back(event);
            doc.decisions.push_back(decision);
        }
    }
    const auto baseline = result;
    if (doc.initialized) {
        for (const auto &old : input.events) {
            if (!protectedEvent(old)) {
                continue;
            }
            auto found =
                std::find_if(result.begin(), result.end(), [&](const auto &e) {
                    return e.note.id == old.note.id;
                });
            if (found == result.end() && old.removed) {
                result.push_back(old);
                continue;
            }
            const bool derived = std::any_of(
                doc.ids.begin(), doc.ids.end(),
                [&](const auto &entry) { return entry.second == old.note.id; });
            if (found == result.end() && old.manual && !derived &&
                std::any_of(doc.regions.begin(), doc.regions.end(),
                            [&](const auto &r) {
                                return r.source != 2 && old.tick >= r.begin &&
                                       old.tick < r.end &&
                                       old.tick + old.duration <= r.end;
                            })) {
                result.push_back(old);
                continue;
            }
            // Deletion overrides remain tombstones while the source still
            // belongs to this recipe.
            const auto decision = std::find_if(
                doc.decisions.begin(), doc.decisions.end(),
                [&](const auto &d) { return d.id == old.note.id && d.copied; });
            const bool fits =
                decision != doc.decisions.end() &&
                std::any_of(
                    doc.regions.begin(), doc.regions.end(), [&](const auto &r) {
                        return r.source == decision->source &&
                               old.tick >= r.begin && old.tick < r.end &&
                               old.tick + old.duration <= r.end;
                    });
            if (found == result.end() || !fits) {
                throw std::runtime_error(
                    "Merge conflicts with protected output note " +
                    std::to_string(old.note.id) +
                    "; restore/unlock that edit explicitly before changing the "
                    "recipe");
            }
            *found = old;
            decision->duration = old.duration;
            decision->tick = old.tick;
            decision->copied = !old.removed;
            decision->reason = old.removed ? "Preserved manual deletion"
                                           : "Preserved protected output edit";
        }
    }
    state.events = std::move(result);
    state.original = baseline;
    state.patterns.clear();
    doc.initialized = true;
    synchronize(state);
    // Same-lane edited sustain conflicts must not be silently fixed on export.
    for (const auto &e : state.events) {
        const bool collision = std::any_of(
            state.events.begin(), state.events.end(), [&](const auto &next) {
                return !next.removed && next.note.id != e.note.id &&
                       next.tick >= e.tick &&
                       next.tick <= e.tick + e.duration &&
                       (next.note.flags & e.note.flags) != 0;
            });
        if (protectedEvent(e) && !e.removed && collision) {
            throw std::runtime_error(
                "Merge would truncate a protected output sustain");
        }
    }
    for (const auto &phrase : input.plus.phrases) {
        if (!phrase.manual && phrase.status == 0) {
            continue;
        }
        for (const auto &old : input.events) {
            if (old.removed || old.tick < phrase.begin ||
                old.tick >= phrase.end) {
                continue;
            }
            const bool kept =
                std::any_of(state.events.begin(), state.events.end(),
                            [&](const auto &event) {
                                return !event.removed &&
                                       event.note.id == old.note.id &&
                                       event.tick >= phrase.begin &&
                                       event.tick < phrase.end;
                            });
            if (!kept) {
                throw std::runtime_error(
                    "Merge removes an attack from protected Plus phrase " +
                    std::to_string(phrase.id) +
                    "; edit/unlock Plus explicitly");
            }
        }
    }
    if (state.plus.settings.enabled) {
        state.plus = proposePlus(state, state.plus.settings).document;
    }
    const auto problems = plusProblems(state);
    if (!problems.empty()) {
        throw std::runtime_error(problems.front());
    }
    validate(state);
    return proposal;
}

void generateMerged(State &state, const std::vector<Pattern> &library) {
    const auto found = state.merges.find(state.partId);
    if (found == state.merges.end()) {
        generate(state, library);
        return;
    }
    state = proposeMerge(state, found->second.config, library, true).state;
}

std::string mergeReport(const State &state) {
    std::ostringstream out;
    {
        cereal::JSONOutputArchive archive(out);
        archive(cereal::make_nvp("merge", state.merges));
    }
    return out.str();
}

std::string mergeRecipe(const State &state) {
    int version = 1;
    std::map<std::string, MergeConfig> recipes;
    for (const auto *part : parts(state)) {
        const auto found = state.merges.find(part->partId);
        if (found != state.merges.end()) {
            recipes[partSection(part->settings)] = found->second.config;
        }
    }
    std::ostringstream out;
    {
        cereal::JSONOutputArchive archive(out);
        archive(CEREAL_NVP(version), CEREAL_NVP(recipes));
    }
    return out.str();
}
void readMergeRecipe(State &state, const std::string &text) {
    int version = 0;
    std::map<std::string, MergeConfig> recipes;
    std::istringstream input(text);
    cereal::JSONInputArchive archive(input);
    archive(CEREAL_NVP(version), CEREAL_NVP(recipes));
    if (version != 1) {
        throw std::runtime_error("Unsupported merge recipe version");
    }
    for (const auto &entry : recipes) {
        const auto id = findPart(state, entry.first);
        auto selected = partState(state, id);
        validateMerge(selected, entry.second);
        state.merges[id].config = entry.second;
    }
}
} // namespace ChartStudio
