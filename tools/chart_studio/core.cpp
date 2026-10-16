#include "core.h"
#include "MidiInput.h"
#include "merge.h"
#include <MidiFile.h>
#include <algorithm>
#include <cereal/external/rapidjson/document.h>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#define XXH_INLINE_ALL
#include <xxhash.h>

namespace ChartStudio {
namespace {
void require(bool ok, const char *message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}
std::string fraction(int64_t ticks, int ppq) {
    const auto divisor = std::gcd(ticks, int64_t(ppq));
    return std::to_string(ticks / divisor) + "/" +
           std::to_string(ppq / divisor);
}
} // namespace

std::string hash(const std::string &bytes) {
    const auto value = XXH3_128bits(bytes.data(), bytes.size());
    XXH128_canonical_t canonical;
    XXH128_canonicalFromHash(&canonical, value);
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto byte : canonical.digest) {
        out << std::setw(2) << unsigned(byte);
    }
    return out.str();
}

std::string fileHash(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot read " + path.u8string());
    }
    XXH3_state_t state;
    XXH3_128bits_reset(&state);
    std::array<char, 65536> buffer;
    while (in) {
        in.read(buffer.data(), buffer.size());
        XXH3_128bits_update(&state, buffer.data(), size_t(in.gcount()));
    }
    if (!in.eof()) {
        throw std::runtime_error("Cannot finish reading " + path.u8string());
    }
    const auto value = XXH3_128bits_digest(&state);
    XXH128_canonical_t canonical;
    XXH128_canonicalFromHash(&canonical, value);
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto byte : canonical.digest) {
        out << std::setw(2) << unsigned(byte);
    }
    return out.str();
}

double Score::seconds(int64_t tick) const {
    double result = 0;
    int64_t previous = 0;
    int us = 500000;
    for (const auto &tempo : tempos) {
        if (tempo.tick > tick) {
            break;
        }
        result += double(tempo.tick - previous) * us / (1000000.0 * ppq);
        previous = tempo.tick;
        us = tempo.microseconds;
    }
    return result + double(tick - previous) * us / (1000000.0 * ppq);
}

int64_t Score::ticks(double time) const {
    require(std::isfinite(time) && time >= 0, "Invalid musical position");
    double position = 0;
    int64_t previous = 0;
    int us = 500000;
    for (const auto &tempo : tempos) {
        const double next =
            position + double(tempo.tick - previous) * us / (1000000.0 * ppq);
        if (next > time) {
            break;
        }
        position = next;
        previous = tempo.tick;
        us = tempo.microseconds;
    }
    const double tick = static_cast<double>(previous) +
                        (time - position) * (1000000.0 * ppq) / us;
    require(tick <= std::numeric_limits<int>::max(),
            "Musical position exceeds MIDI tick range");
    return int64_t(std::llround(tick));
}

void Score::validate() const {
    require(ppq > 0 && ppq < 32768 && !tempos.empty(), "Invalid tempo map");
    int64_t previous = -1;
    for (const auto &tempo : tempos) {
        require(tempo.tick > previous && tempo.microseconds > 0 &&
                    tempo.microseconds <= 16777215,
                "Invalid tempo event");
        previous = tempo.tick;
    }
    require(tempos.front().tick == 0, "Tempo map must begin at zero");
    for (const auto &signature : signatures) {
        require(signature.tick >= 0 && signature.numerator > 0 &&
                    signature.denominator > 0 &&
                    signature.denominator <= 1024 &&
                    (signature.denominator & (signature.denominator - 1)) == 0,
                "Invalid time signature");
    }
    std::set<uint64_t> ids;
    for (const auto &track : tracks) {
        for (const auto &note : track.notes) {
            require(note.id && ids.insert(note.id).second && note.start >= 0 &&
                        note.end >= note.start &&
                        note.end <= std::numeric_limits<int>::max() &&
                        note.pitch >= 0 && note.pitch <= 127 &&
                        note.channel >= 0 && note.channel <= 15 &&
                        note.program >= 0 && note.program <= 127 &&
                        note.velocity >= 1 && note.velocity <= 127,
                    "Invalid musical note");
        }
    }
}

Score importMidi(const fs::path &path) {
    const auto size = fs::file_size(path);
    require(size >= 14 && size <= 64ULL * 1024 * 1024,
            "MIDI file must be 14 bytes..64 MiB");
    std::ifstream input(path, std::ios::binary);
    std::vector<unsigned char> bytes(size);
    require(bool(input.read(reinterpret_cast<char *>(bytes.data()),
                            std::streamsize(size))),
            "Cannot read MIDI");
    Score result;
    result.ppq = gpp::midi::validateSmf(bytes);
    std::istringstream stream(std::string(bytes.begin(), bytes.end()),
                              std::ios::binary);
    smf::MidiFile midi;
    require(midi.read(stream), "midifile rejected MIDI");
    midi.doTimeAnalysis();
    midi.linkNotePairs();
    std::map<int64_t, Tempo> tempos{{0, {}}};
    std::map<int64_t, Signature> signatures{{0, {}}};
    uint64_t id = 1;
    for (int t = 0; t < midi.getTrackCount(); ++t) {
        Track track;
        std::array<int, 16> programs{};
        for (int e = 0; e < midi[t].size(); ++e) {
            auto &event = midi[t][e];
            if (event.isMeta() && event.size() >= 3 && event[1] == 0x06) {
                result.markers.push_back({event.tick, event.getMetaContent()});
            }
            if (event.isTrackName()) {
                track.name = event.getMetaContent();
            }
            if (event.isPatchChange()) {
                programs[event.getChannel()] = event.getP1();
            }
            if (event.isTempo()) {
                const auto data = event.getMetaContent();
                const int us = (int(uint8_t(data[0])) << 16) |
                               (int(uint8_t(data[1])) << 8) |
                               int(uint8_t(data[2]));
                tempos[event.tick] = {event.tick, us};
            }
            if (event.isTimeSignature()) {
                const auto data = event.getMetaContent();
                require(uint8_t(data[0]) > 0 && uint8_t(data[1]) <= 10,
                        "Invalid MIDI time signature values");
                signatures[event.tick] = {event.tick, int(uint8_t(data[0])),
                                          1 << uint8_t(data[1])};
            }
            if (event.isNoteOn()) {
                if (!event.getLinkedEvent()) {
                    throw std::runtime_error("Unclosed musical note in track " +
                                             std::to_string(t) + " at tick " +
                                             std::to_string(event.tick));
                }
                track.notes.push_back({id++, event.getKeyNumber(),
                                       event.getVelocity(), event.getChannel(),
                                       programs[event.getChannel()], event.tick,
                                       event.getLinkedEvent()->tick});
            }
        }
        std::stable_sort(track.notes.begin(), track.notes.end(),
                         [](const auto &a, const auto &b) {
                             return a.start != b.start ? a.start < b.start
                                                       : a.id < b.id;
                         });
        result.tracks.push_back(std::move(track));
    }
    result.tempos.clear();
    result.signatures.clear();
    for (const auto &item : tempos) {
        result.tempos.push_back(item.second);
    }
    for (const auto &item : signatures) {
        result.signatures.push_back(item.second);
    }
    result.warnings.push_back("Durations follow MIDI Note Off; pedals, bends "
                              "and expression are not synthesized.");
    result.validate();
    return result;
}

void Settings::validate() const {
    (void)partSection(*this);
    require(difficulty == "expert" || difficulty == "hard" ||
                difficulty == "medium" || difficulty == "easy",
            "Unknown difficulty");
    require(algorithm == "beam" || algorithm == "heuristic" ||
                algorithm == "umap" || algorithm.rfind("umap:", 0) == 0,
            "Unknown algorithm");
    require(channel >= -1 && channel <= 15 && program >= -1 && program <= 127 &&
                maxChord >= 1 && maxChord <= 3 && beam >= 1 && beam <= 256,
            "Invalid mapping bounds");
    require(std::isfinite(offset) && std::abs(offset) <= 36000 &&
                std::isfinite(minimumSustain) && minimumSustain >= -128 &&
                minimumSustain <= 60 && std::isfinite(density) && density > 0 &&
                density <= 1,
            "Invalid timing/density settings");
    for (int weight : weights) {
        require(weight >= 0 && weight <= 10000, "Cost weight must be 0..10000");
    }
}

std::string Settings::fingerprint() const {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "mapping-v1:" << difficulty << ':' << algorithm << ':' << maxChord
        << ':' << beam << ':' << std::setprecision(17) << density;
    for (int weight : weights) {
        out << ':' << weight;
    }
    return out.str();
}

std::string canonical(const std::vector<Event> &events, size_t begin,
                      size_t count, const Score &score,
                      const Settings &settings, bool relative) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "signature-v1/" << settings.instrument << "/" << settings.difficulty
        << '/' << count << '/';
    const int64_t start = events.at(begin).tick;
    int base = 127;
    for (const auto &source : events[begin].sources) {
        base = std::min(base, source.pitch);
    }
    if (!relative) {
        base = 0;
    }
    for (size_t i = begin; i < begin + count; ++i) {
        out << fraction(events[i].tick - start, score.ppq) << ':';
        std::vector<std::tuple<int, int64_t, int>> members;
        for (const auto &source : events[i].sources) {
            // Program family expresses role; track/channel numbers are source
            // identities, not motif identities.
            members.emplace_back(source.pitch - base, source.end - source.start,
                                 source.program / 8);
        }
        std::sort(members.begin(), members.end());
        for (const auto &member : members) {
            out << std::get<0>(member) << ','
                << fraction(std::get<1>(member), score.ppq) << ','
                << std::get<2>(member) << ';';
        }
        out << '|';
    }
    return out.str();
}

std::string inspect(const Score &score) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "PPQ " << score.ppq << ", tempos " << score.tempos.size()
        << ", signatures " << score.signatures.size() << '\n';
    for (size_t i = 0; i < score.tracks.size(); ++i) {
        const auto &track = score.tracks[i];
        std::map<std::pair<int, int>, std::vector<const ScoreNote *>> sources;
        out << "Track " << i << " \"" << track.name
            << "\" notes=" << track.notes.size() << '\n';
        for (const auto &n : track.notes) {
            sources[{n.channel, n.program}].push_back(&n);
        }
        for (const auto &source : sources) {
            int low = 127, high = 0;
            int64_t end = 0;
            std::map<int64_t, int> attacks;
            for (const auto *note : source.second) {
                low = std::min(low, note->pitch);
                high = std::max(high, note->pitch);
                end = std::max(end, note->end);
                ++attacks[note->start];
            }
            size_t chords = 0;
            for (const auto &attack : attacks) {
                chords += attack.second > 1;
            }
            const double duration = score.seconds(end);
            out << "  channel=" << source.first.first
                << " program=" << source.first.second << " "
                << (source.first.first == 9
                        ? "PERCUSSION"
                        : smf::MidiFile::getGMInstrumentName(
                              source.first.second))
                << " notes=" << source.second.size()
                << " attacks=" << attacks.size() << " chords=" << chords
                << " pitches=" << low << ".." << high
                << " duration=" << duration << " attacks/s="
                << (duration > 0
                        ? static_cast<double>(attacks.size()) / duration
                        : 0)
                << '\n';
        }
    }
    return out.str();
}

std::string encode(const State &state) {
    std::ostringstream out;
    {
        cereal::JSONOutputArchive archive(out);
        archive(state);
        archive(cereal::make_nvp("synth", state.synth));
        archive(cereal::make_nvp("alignment", state.alignment));
        archive(cereal::make_nvp("merges", state.merges));
        archive(cereal::make_nvp("plus", state.plus));
        archive(cereal::make_nvp("musical_markers", state.score.markers));
        archive(cereal::make_nvp("instrument", state.settings.instrument),
                cereal::make_nvp("active_part_id", state.partId),
                cereal::make_nvp("next_part_id", state.nextPartId),
                cereal::make_nvp("parts", state.otherParts));
    }
    return out.str();
}

State decode(const std::string &text) {
    State state;
    std::istringstream input(text);
    {
        cereal::JSONInputArchive archive(input);
        archive(state);
        rapidjson::Document document;
        document.Parse(text.c_str());
        if (!document.HasParseError() && document.IsObject() &&
            document.HasMember("alignment")) {
            archive(cereal::make_nvp("alignment", state.alignment));
        }
    }
    rapidjson::Document extra;
    extra.Parse(text.c_str());
    if (!extra.HasParseError() && extra.IsObject() && extra.HasMember("plus")) {
        std::istringstream plusInput(text);
        cereal::JSONInputArchive archive(plusInput);
        archive(cereal::make_nvp("plus", state.plus));
    }
    if (!extra.HasParseError() && extra.IsObject() &&
        extra.HasMember("musical_markers")) {
        std::istringstream markerInput(text);
        cereal::JSONInputArchive archive(markerInput);
        archive(cereal::make_nvp("musical_markers", state.score.markers));
    }
    if (!extra.HasParseError() && extra.IsObject() &&
        extra.HasMember("parts")) {
        std::istringstream partsInput(text);
        cereal::JSONInputArchive archive(partsInput);
        archive(cereal::make_nvp("instrument", state.settings.instrument),
                cereal::make_nvp("active_part_id", state.partId),
                cereal::make_nvp("next_part_id", state.nextPartId),
                cereal::make_nvp("parts", state.otherParts));
    }
    if (!extra.HasParseError() && extra.IsObject() &&
        extra.HasMember("merges")) {
        std::istringstream mergeInput(text);
        cereal::JSONInputArchive archive(mergeInput);
        archive(cereal::make_nvp("merges", state.merges));
    }
    if (!extra.HasParseError() && extra.IsObject() &&
        extra.HasMember("synth")) {
        std::istringstream synthInput(text);
        cereal::JSONInputArchive archive(synthInput);
        archive(cereal::make_nvp("synth", state.synth));
    }
    validate(state);
    return state;
}

double audioSeconds(const State &state, int64_t tick) {
    return state.alignment.scale * state.score.seconds(tick) +
           state.settings.offset;
}

int64_t audioTick(const State &state, double time) {
    return state.score.ticks(
        std::max(0.0, (time - state.settings.offset) / state.alignment.scale));
}

int64_t exportMilliBpm(const State &state, const Tempo &tempo) {
    return std::llround(60000000000.0 /
                        (tempo.microseconds * state.alignment.scale));
}

void synchronize(State &state) {
    auto sync = [&](Part &part) {
        part.settings.offset = state.settings.offset;
        for (auto &event : part.events) {
            require(event.tick >= 0 && event.duration >= 0 &&
                        event.duration <= INT32_MAX &&
                        event.tick <= INT32_MAX - event.duration,
                    "Mapped event exceeds MIDI tick range");
            event.note.time = audioSeconds(state, event.tick);
            event.note.duration =
                audioSeconds(state, event.tick + event.duration) -
                audioSeconds(state, event.tick);
        }
        std::stable_sort(part.events.begin(), part.events.end(),
                         [](const Event &a, const Event &b) {
                             return a.tick != b.tick ? a.tick < b.tick
                                                     : a.note.id < b.note.id;
                         });
    };
    sync(state);
    for (auto &part : state.otherParts) {
        sync(part);
    }
}

namespace {
void validatePart(const Part &state) {
    state.plus.settings.validate();
    require(state.plus.version == 1, "Unsupported Plus document version");
    std::set<uint64_t> plusIds;
    for (const auto &phrase : state.plus.phrases) {
        require(phrase.id > 0 && phrase.id < state.plus.nextId &&
                    plusIds.insert(phrase.id).second && phrase.begin >= 0 &&
                    phrase.end > phrase.begin && phrase.end <= INT32_MAX &&
                    phrase.status >= 0 && phrase.status <= 2 &&
                    std::isfinite(phrase.risk),
                "Invalid Plus phrase data");
    }
    state.settings.validate();
    std::set<uint64_t> ids;
    for (const auto &e : state.events) {
        require(
            e.note.id && e.note.id < state.nextId &&
                ids.insert(e.note.id).second && e.tick >= 0 &&
                std::isfinite(e.note.time) && std::isfinite(e.note.duration) &&
                e.note.duration >= 0 && e.duration >= 0 &&
                e.duration <= std::numeric_limits<int>::max() &&
                e.tick <= std::numeric_limits<int>::max() - e.duration &&
                e.note.flags >= 0 && e.note.flags <= 31 &&
                (e.note.flags || e.removed) && e.status >= 0 && e.status <= 2,
            "Invalid mapped event");
    }
    for (const auto &entry : state.patterns) {
        const auto &p = entry.second;
        require(entry.first == p.exact && !p.exact.empty() &&
                    p.masks.size() <= 32 && p.status >= 0 && p.status <= 2,
                "Invalid pattern");
        for (int mask : p.masks) {
            require(mask >= 0 && mask <= 31, "Invalid pattern lanes");
        }
    }
}

} // namespace

void validate(const State &state) {
    state.score.validate();
    require(std::isfinite(state.alignment.scale) &&
                state.alignment.scale >= .5 && state.alignment.scale <= 2,
            "Invalid alignment scale");
    require(state.otherParts.size() < 20,
            "At most twenty instrument/difficulty parts are supported");
    std::set<uint64_t> ids;
    std::set<std::string> sections;
    for (const auto &entry : state.merges) {
        require(entry.first == state.partId ||
                    std::any_of(state.otherParts.begin(),
                                state.otherParts.end(),
                                [&](const auto &part) {
                                    return part.partId == entry.first;
                                }),
                "Orphan merge recipe");
    }
    for (const auto *part : parts(state)) {
        require(part->partId > 0 && part->partId < state.nextPartId &&
                    ids.insert(part->partId).second,
                "Invalid or duplicate part ID");
        require(sections.insert(partSection(part->settings)).second,
                "Duplicate chart section");
        require(part->settings.offset == state.settings.offset,
                "Parts must share the audio offset");
        validatePart(*part);
        const auto merge = state.merges.find(part->partId);
        if (merge != state.merges.end()) {
            const auto selected = partState(state, part->partId);
            validateMerge(selected, merge->second.config);
            if (merge->second.initialized) {
                validatePart(merge->second.primary);
                validatePart(merge->second.secondary);
                for (const auto &id : merge->second.ids) {
                    require(id.second > 0 && id.second < part->nextId,
                            "Invalid merge output identity");
                }
            }
        }
    }
}

std::string report(const State &state) {
    size_t attacks = 0, chords = 0, sustains = 0, reused = 0, removed = 0,
           simplified = 0, sustainLosses = 0, shortSustains = 0,
           shortenedSustains = 0;
    std::set<std::string> phrases, novel;
    std::map<int64_t, int> density;
    double end = 0;
    for (const auto &event : state.events) {
        if (event.removed) {
            ++removed;
            continue;
        }
        ++attacks;
        chords += (event.note.flags & (event.note.flags - 1)) != 0;
        sustains += exportDuration(state, event) > 0;
        shortenedSustains += exportDuration(state, event) < event.duration;
        shortSustains +=
            event.duration > 0 && exportDuration(state, event) == 0;
        reused += event.origin != "algorithm";
        phrases.insert(event.phrase);
        if (event.origin == "algorithm") {
            novel.insert(event.exact);
        }
        int laneCount = 0;
        for (int mask = event.note.flags; mask; mask &= mask - 1) {
            ++laneCount;
        }
        std::set<int> sourcePitches;
        for (const auto &source : event.sources) {
            sourcePitches.insert(source.pitch);
            sustainLosses += source.end - source.start != event.duration;
        }
        simplified += sourcePitches.size() > size_t(laneCount);
        end = std::max(end, event.note.time + event.note.duration);
        ++density[int64_t(std::floor(event.note.time))];
    }
    int peak = 0;
    for (const auto &bin : density) {
        peak = std::max(peak, bin.second);
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "{\"schema\":1,\"attacks\":" << attacks << ",\"chords\":" << chords
        << ",\"sustains\":" << sustains << ",\"phrases\":" << phrases.size()
        << ",\"new_patterns\":" << novel.size()
        << ",\"reused_events\":" << reused << ",\"reuse_ratio\":"
        << (attacks ? static_cast<double>(reused) / static_cast<double>(attacks)
                    : 0)
        << ",\"discarded\":" << removed
        << ",\"chord_reductions\":" << simplified
        << ",\"sustain_duration_losses\":" << sustainLosses
        << ",\"short_sustains_removed\":" << shortSustains
        << ",\"export_sustains_shortened\":" << shortenedSustains
        << ",\"alignment_scale\":" << state.alignment.scale
        << ",\"offset_seconds\":" << state.settings.offset
        << ",\"conflicts\":" << state.conflicts.size()
        << ",\"average_attacks_per_second\":"
        << (end > 0 ? static_cast<double>(attacks) / end : 0)
        << ",\"peak_attacks_in_second_bin\":" << peak << "}\n";
    return out.str();
}
} // namespace ChartStudio
