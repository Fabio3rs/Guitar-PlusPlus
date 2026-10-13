#include "CMidiChart.h"
#include "ChartHopo.h"
#include <MidiFile.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
using Bytes = std::vector<unsigned char>;

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n\0", 0, 5);
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n\0", std::string::npos, 5);
    return value.substr(first, last - first + 1);
}

std::string lower(std::string value) {
    for (char &c : value) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return value;
}

void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Upstream deliberately trusts chunk lengths, permits missing EOT, allocates
// from untrusted lengths and accumulates ticks in int. Check those boundaries
// before read(); this pass stores no musical events and performs no decoding
// into game data. Limits comfortably exceed normal multi-instrument charts.
int validateSmf(const Bytes &bytes) {
    size_t pos = 0, end = bytes.size(), events = 0;
    auto byte = [&]() -> unsigned {
        require(pos < end, "Truncated MIDI data");
        return bytes[pos++];
    };
    auto integer = [&](int count) {
        uint32_t value = 0;
        for (int i = 0; i < count; ++i) {
            value = (value << 8) | byte();
        }
        return value;
    };
    auto vlq = [&]() {
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            const auto b = byte();
            value = (value << 7) | (b & 127);
            if (b < 128) {
                return value;
            }
        }
        throw std::runtime_error("MIDI VLQ exceeds four bytes");
    };
    require(integer(4) == 0x4d546864, "Not a Standard MIDI File (MThd)");
    require(integer(4) == 6, "Unsupported MIDI header length");
    const auto type = integer(2), tracks = integer(2), ppq = integer(2);
    require(type <= 1, "SMF Type 2 is not a gameplay timeline");
    require(tracks > 0 && tracks <= 256 && (type != 0 || tracks == 1),
            "Invalid or excessive MIDI track count");
    require(ppq > 0 && ppq < 0x8000,
            "MIDI requires positive PPQ; SMPTE is unsupported");
    for (unsigned track = 0; track < tracks; ++track) {
        require(integer(4) == 0x4d54726b, "Missing MIDI track chunk (MTrk)");
        const auto length = integer(4);
        require(length <= bytes.size() - pos, "Truncated MIDI track chunk");
        end = pos + length;
        unsigned running = 0;
        uint64_t tick = 0;
        bool ended = false;
        while (pos < end) {
            require(++events <= 2000000, "MIDI event budget exceeded");
            tick += vlq();
            require(tick <=
                        static_cast<uint64_t>(std::numeric_limits<int>::max()),
                    "MIDI tick exceeds midifile integer range");
            unsigned status = byte();
            if (status < 128) {
                require(running >= 0x80 && running < 0xf0,
                        "Invalid MIDI running status");
                --pos;
                status = running;
            }
            if (status < 0xf0) {
                running = status;
                const int size = ((status & 0xe0) == 0xc0) ? 1 : 2;
                for (int i = 0; i < size; ++i) {
                    require(byte() < 128, "Invalid MIDI channel data");
                }
            } else {
                running = 0;
                require(status == 0xff || status == 0xf0 || status == 0xf7,
                        "Unsupported system status in SMF");
                const auto meta = status == 0xff ? byte() : 0;
                const auto size = vlq();
                require(size <= end - pos, "Truncated MIDI event payload");
                if (status == 0xff && meta == 0x51) {
                    require(size == 3, "Invalid tempo payload");
                    require((bytes[pos] | bytes[pos + 1] | bytes[pos + 2]) != 0,
                            "Zero MIDI tempo");
                }
                if (status == 0xff && meta == 0x58) {
                    require(size == 4, "Invalid time signature payload");
                }
                pos += size;
                if (status == 0xff && meta == 0x2f) {
                    require(size == 0 && pos == end,
                            "Invalid end-of-track boundary");
                    ended = true;
                }
            }
        }
        require(ended, "Missing MIDI end-of-track event");
        end = bytes.size();
    }
    require(pos == bytes.size(), "Unexpected data after MIDI tracks");
    return static_cast<int>(ppq);
}

std::map<std::string, std::string>
readIni(const std::filesystem::path &directory) {
    std::vector<std::filesystem::path> paths;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(directory, ec), last;
         !ec && it != last; it.increment(ec)) {
        if (lower(it->path().filename().string()) == "song.ini") {
            paths.push_back(it->path());
        }
    }
    std::sort(paths.begin(), paths.end());
    std::map<std::string, std::string> result;
    if (paths.empty()) {
        return result;
    }
    std::ifstream input(paths.front());
    std::string line;
    bool song = false;
    while (std::getline(input, line)) {
        if (line.compare(0, 3, "\xef\xbb\xbf") == 0) {
            line.erase(0, 3);
        }
        line = trim(line);
        if (line.empty() || line.front() == ';' || line.front() == '#') {
            continue;
        }
        if (line.front() == '[') {
            song = lower(line) == "[song]";
            continue;
        }
        const auto equals = line.find('=');
        if (song && equals != std::string::npos) {
            auto value = trim(line.substr(equals + 1));
            if (value.size() >= 2 && value.front() == '"' &&
                value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            result[lower(trim(line.substr(0, equals)))] = value;
        }
    }
    return result;
}

struct Interval {
    int begin = 0, end = 0, key = 0;
    double seconds = 0, duration = 0;
};

bool covers(const Interval &interval, int64_t tick) {
    return interval.begin <= tick && tick < interval.end;
}

std::string instrumentName(const std::string &name) {
    if (name == "part guitar" || name == "t1 gems") {
        return "Single";
    }
    if (name == "part bass") {
        return "DoubleBass";
    }
    return {};
}
} // namespace

bool readMidiChart(const std::filesystem::path &path, MidiChart &result,
                   std::string &error) try {
    result = {};
    error.clear();
    const auto size = std::filesystem::file_size(path);
    require(size >= 14 && size <= 64ULL * 1024 * 1024,
            "Invalid MIDI file size (limit 64 MiB)");
    std::ifstream input(path, std::ios::binary);
    Bytes bytes(static_cast<size_t>(size));
    require(static_cast<bool>(
                input.read(reinterpret_cast<char *>(bytes.data()),
                           static_cast<std::streamsize>(bytes.size()))),
            "Cannot read MIDI file");
    MidiChart loaded;
    loaded.source.ppq = validateSmf(bytes);
    std::istringstream stream(std::string(bytes.begin(), bytes.end()),
                              std::ios::binary);
    smf::MidiFile midi;
    require(midi.read(stream), "midifile rejected SMF");
    midi.doTimeAnalysis();
    midi.linkNotePairs();

    const auto ini = readIni(path.has_parent_path() ? path.parent_path() : ".");
    auto metadata = [&](const char *key) {
        const auto it = ini.find(key);
        return it == ini.end() ? std::string() : it->second;
    };
    auto number = [&](const char *key, double fallback, bool negative) {
        const auto value = metadata(key);
        if (value.empty()) {
            return fallback;
        }
        size_t used = 0;
        double parsed = 0.0;
        try {
            parsed = std::stod(value, &used);
        } catch (const std::exception &) {
            used = std::string::npos;
        }
        if (used == value.size() && std::isfinite(parsed) &&
            (negative || parsed >= 0.0) && std::abs(parsed) <= 2147483647.0) {
            return parsed;
        }
        loaded.source.warnings.push_back(std::string("Invalid song.ini ") +
                                         key + "; using default");
        return fallback;
    };
    loaded.name = metadata("name");
    loaded.artist = metadata("artist");
    loaded.charter = metadata("charter");
    loaded.source.offset = number("delay", 0.0, true) / 1000.0;
    const int ppq = loaded.source.ppq;
    int threshold = ppq / 3 + 1;
    loaded.source.hopoPolicy = "midi-community";
    const auto eighth = lower(metadata("eighthnote_hopo"));
    if (eighth == "true" || eighth == "1") {
        threshold = ppq / 2 + 1;
        loaded.source.hopoPolicy = "eighthnote_hopo";
    }
    const int configuredHopo =
        static_cast<int>(number("hopo_frequency", 0.0, false));
    if (configuredHopo > 0) {
        threshold = configuredHopo;
        loaded.source.hopoPolicy = "hopo_frequency";
    }
    loaded.source.hopoThresholdTicks = threshold;
    if (!metadata("hopofreq").empty()) {
        loaded.source.warnings.push_back(
            "Legacy hopofreq is unsupported; use hopo_frequency");
    }
    const double sustainCutoff =
        number("sustain_cutoff_threshold", ppq / 3.0, false);
    int plusKey = static_cast<int>(number("multiplier_note", 0.0, false));
    if (!metadata("star_power_note").empty()) {
        plusKey = static_cast<int>(number("star_power_note", plusKey, false));
    }
    if (plusKey != 0 && plusKey != 103 && plusKey != 116) {
        loaded.source.warnings.push_back(
            "Plus marker override must be 103 or 116; using auto");
        plusKey = 0;
    }

    // Capture names before interpreting any notes, even if a name occurs late.
    for (int track = 0; track < midi.getTrackCount(); ++track) {
        std::string name;
        for (int i = 0; i < midi[track].size(); ++i) {
            const auto &event = midi[track][i];
            if (event.isMeta() && event.getMetaType() == 3) {
                name = trim(event.getMetaContent());
                break;
            }
        }
        loaded.trackNames.push_back(name);
    }
    if (loaded.name.empty()) {
        loaded.name = path.parent_path().filename().string();
    }

    struct TimedTempo {
        int tick, sequence;
        double seconds, bpm;
    };
    std::vector<TimedTempo> tempos;
    for (int track = 0; track < midi.getTrackCount(); ++track) {
        for (int i = 0; i < midi[track].size(); ++i) {
            const auto &event = midi[track][i];
            require(std::isfinite(event.seconds), "Non-finite MIDI event time");
            if (event.isTempo()) {
                tempos.push_back({event.tick, event.seq, event.seconds,
                                  event.getTempoBPM()});
            } else if (event.isMeta()) {
                const int type = event.getMetaType();
                if (type == 0x58) {
                    loaded.source.events.push_back(
                        {event.tick, event.seconds + loaded.source.offset,
                         loaded.trackNames[track], "time-signature",
                         std::to_string(event[3]) + "/2^" +
                             std::to_string(event[4])});
                } else if (type >= 1 && type <= 7) {
                    const auto text = event.getMetaContent();
                    const auto kind =
                        text.find("section ") != std::string::npos ||
                                text.find("prc_") != std::string::npos
                            ? "section"
                            : "text";
                    loaded.source.events.push_back(
                        {event.tick, event.seconds + loaded.source.offset,
                         loaded.trackNames[track], kind, text});
                }
            }
        }
    }
    std::stable_sort(
        tempos.begin(), tempos.end(), [](const auto &a, const auto &b) {
            return a.tick != b.tick ? a.tick < b.tick : a.sequence < b.sequence;
        });
    CChart::Note initial;
    initial.time = loaded.source.offset;
    initial.lTime = 120;
    loaded.bpm.push_back(initial);
    for (const auto &tempo : tempos) {
        loaded.source.tempos.push_back(
            {tempo.tick, tempo.seconds + loaded.source.offset, tempo.bpm});
        CChart::Note entry;
        entry.time = tempo.seconds + loaded.source.offset;
        entry.lTime = tempo.bpm;
        if (loaded.bpm.back().time == entry.time) {
            loaded.bpm.back() = entry;
        } else {
            loaded.bpm.push_back(entry);
        }
    }

    const std::array<const char *, 4> difficulties{"Easy", "Medium", "Hard",
                                                   "Expert"};
    for (int track = 0; track < midi.getTrackCount(); ++track) {
        const auto &trackName = loaded.trackNames[track];
        const auto instrument = instrumentName(lower(trackName));
        if (instrument.empty()) {
            continue;
        }
        std::vector<Interval> notes;
        std::array<std::vector<Interval>, 4> sysex;
        std::map<std::pair<int, int>, int> activeSysex;
        bool enhancedOpens = false;
        std::set<std::string> warnings;
        for (int i = 0; i < midi[track].size(); ++i) {
            const auto &event = midi[track][i];
            if (event.isMeta() && event.getMetaType() == 1) {
                const auto text = lower(trim(event.getMetaContent()));
                enhancedOpens |=
                    text == "[enhanced_opens]" || text == "enhanced_opens";
            }
        }
        for (int i = 0; i < midi[track].size(); ++i) {
            auto &event = midi[track][i];
            if (event.isNoteOn()) {
                const int key = event.getKeyNumber();
                if (!event.isLinked()) {
                    const int relative = key - 60;
                    const bool gemOrForce =
                        relative >= 0 && relative <= 42 && relative % 12 <= 6;
                    const bool open = enhancedOpens && key >= 59 && key <= 95 &&
                                      (key - 59) % 12 == 0;
                    if (gemOrForce || open) {
                        throw std::runtime_error(
                            trackName + ": unclosed playable note/modifier");
                    }
                    warnings.insert("Unclosed non-playable marker ignored");
                    continue;
                }
                const auto *off = event.getLinkedEvent();
                require(off->tick >= event.tick, "Negative MIDI note duration");
                notes.push_back({event.tick, off->tick, key,
                                 event.seconds + loaded.source.offset,
                                 event.getDurationInSeconds()});
            } else if (!event.empty() &&
                       (event[0] == 0xf0 || event[0] == 0xf7)) {
                // Phase Shift: F0 50 53 00 00 difficulty phrase state F7.
                if (event.size() != 9 || event[1] != 0x50 || event[2] != 0x53 ||
                    event[3] != 0 || event[4] != 0 || event[8] != 0xf7 ||
                    (event[5] > 3 && event[5] != 0xff) ||
                    (event[6] != 1 && event[6] != 4) || event[7] > 1) {
                    warnings.insert("Unsupported SysEx ignored");
                    continue;
                }
                for (int difficulty = 0; difficulty < 4; ++difficulty) {
                    if (event[5] != 0xff && event[5] != difficulty) {
                        continue;
                    }
                    const auto key =
                        std::make_pair(difficulty, static_cast<int>(event[6]));
                    if (event[7] == 1) {
                        if (!activeSysex.emplace(key, event.tick).second) {
                            warnings.insert("Repeated SysEx start ignored");
                        }
                    } else {
                        const auto it = activeSysex.find(key);
                        if (it != activeSysex.end()) {
                            sysex[difficulty].push_back(
                                {it->second, event.tick, event[6], 0, 0});
                            activeSysex.erase(it);
                        } else {
                            warnings.insert("Unmatched SysEx end ignored");
                        }
                    }
                }
            }
        }
        if (!activeSysex.empty()) {
            warnings.insert("Unclosed SysEx phrase ignored");
        }
        std::stable_sort(
            notes.begin(), notes.end(),
            [](const auto &a, const auto &b) { return a.begin < b.begin; });
        const bool has116 =
            std::any_of(notes.begin(), notes.end(),
                        [](const auto &n) { return n.key == 116; });
        const int trackPlusKey = plusKey ? plusKey : (has116 ? 116 : 103);
        for (int difficulty = 0; difficulty < 4; ++difficulty) {
            const int base = 60 + difficulty * 12;
            MidiChart::Instrument part;
            part.source = loaded.source;
            part.source.warnings.insert(part.source.warnings.end(),
                                        warnings.begin(), warnings.end());
            std::map<int, std::vector<ChartSourceInfo::Gem>> groups;
            std::vector<Interval> forces, phrases, taps;
            for (const auto &note : notes) {
                if ((note.key >= base && note.key < base + 5) ||
                    (enhancedOpens && note.key == base - 1)) {
                    const int lane = note.key == base - 1
                                         ? nf_open
                                         : (1 << (note.key - base));
                    ChartSourceInfo::Gem gem{note.begin, note.end, lane,
                                             note.seconds, note.duration};
                    part.source.gems.push_back(gem);
                    groups[note.begin].push_back(gem);
                } else if (note.key == base + 5 || note.key == base + 6) {
                    forces.push_back(note);
                    part.source.events.push_back(
                        {note.begin, note.seconds, trackName,
                         note.key == base + 5 ? "force-hopo" : "force-strum",
                         std::to_string(note.key) +
                             " end_tick=" + std::to_string(note.end)});
                } else if (note.key == trackPlusKey) {
                    phrases.push_back(note);
                } else if (note.key == 104) {
                    taps.push_back(note);
                }
                if (note.key >= 103) {
                    part.source.events.push_back(
                        {note.begin, note.seconds, trackName,
                         note.key == trackPlusKey
                             ? "plus"
                             : (note.key == 103 ? "solo" : "midi-marker"),
                         std::to_string(note.key) +
                             " end_tick=" + std::to_string(note.end)});
                }
            }
            for (const auto &phrase : sysex[difficulty]) {
                part.source.events.push_back(
                    {phrase.begin,
                     midi.getTimeInSeconds(phrase.begin) + loaded.source.offset,
                     trackName, phrase.key == 1 ? "sysex-open" : "sysex-tap",
                     "end_tick=" + std::to_string(phrase.end)});
            }
            for (const auto &group : groups) {
                CChart::Note note;
                note.time = group.second.front().seconds;
                double firstLength = -1;
                bool unequal = false;
                for (const auto &gem : group.second) {
                    note.type |= gem.lane;
                    const double length =
                        static_cast<double>(gem.endTick - gem.tick) <
                                sustainCutoff
                            ? 0
                            : gem.duration;
                    unequal |= firstLength >= 0 && length != firstLength;
                    firstLength = length;
                    note.lTime = std::max(note.lTime, length);
                }
                if (unequal) {
                    warnings.insert(
                        "Disjoint chord sustains use the maximum duration");
                }
                bool tap = false, forceHopo = false, forceStrum = false;
                for (const auto &phrase : sysex[difficulty]) {
                    if (covers(phrase, group.first)) {
                        if (phrase.key == 1) {
                            note.type = nf_open;
                        } else {
                            tap = true;
                        }
                    }
                }
                if ((note.type & nf_open) != 0 && (note.type & 31) != 0) {
                    throw std::runtime_error(
                        trackName + ": mixed open/fretted chord unsupported");
                }
                if (note.lTime > 0) {
                    note.type |= nf_slide;
                }
                const auto *previous =
                    part.notes.empty() ? nullptr : &part.notes.back();
                const bool close =
                    previous &&
                    group.first - part.source.noteTicks.back() <= threshold;
                chart::classifyHopo(note, previous, close, true);
                for (const auto &force : forces) {
                    if (covers(force, group.first)) {
                        forceHopo |= force.key == base + 5;
                        forceStrum |= force.key == base + 6;
                    }
                }
                for (const auto &phrase : taps) {
                    tap |= covers(phrase, group.first);
                }
                std::string reason = "natural";
                if (forceHopo) {
                    note.type &= ~nf_not_hopo;
                    reason = "force-hopo";
                }
                if (forceStrum) {
                    note.type |= nf_not_hopo;
                    reason = "force-strum";
                }
                if (forceHopo && forceStrum) {
                    warnings.insert(
                        "Conflicting HOPO/strum markers: strum wins");
                }
                if (tap) {
                    note.type |= noteTap | nf_not_hopo;
                    reason = "tap";
                }
                part.notes.push_back(note);
                part.source.noteTicks.push_back(group.first);
                part.source.hopoReasons.push_back(reason);
            }
            for (const auto &phrase : phrases) {
                const auto first =
                    std::lower_bound(part.source.noteTicks.begin(),
                                     part.source.noteTicks.end(), phrase.begin);
                const auto last = std::lower_bound(
                    first, part.source.noteTicks.end(), phrase.end);
                if (first == last) {
                    continue;
                }
                CChart::plusNote plus;
                plus.time = phrase.seconds;
                plus.lTime = phrase.duration;
                plus.firstNote =
                    static_cast<size_t>(first - part.source.noteTicks.begin());
                plus.lastNote = static_cast<size_t>(
                    last - part.source.noteTicks.begin() - 1);
                if (!part.plus.empty() &&
                    plus.time <
                        part.plus.back().time + part.plus.back().lTime - 1e-9) {
                    throw std::runtime_error(trackName +
                                             ": overlapping Plus phrases");
                }
                part.plus.push_back(plus);
            }
            part.source.warnings.assign(loaded.source.warnings.begin(),
                                        loaded.source.warnings.end());
            part.source.warnings.insert(part.source.warnings.end(),
                                        warnings.begin(), warnings.end());
            if (!part.notes.empty()) {
                const std::string key = "[" +
                                        std::string(difficulties[difficulty]) +
                                        instrument + "]";
                require(loaded.instruments.emplace(key, std::move(part)).second,
                        "Duplicate supported MIDI instrument track");
            }
        }
    }
    if (loaded.instruments.empty()) {
        loaded.source.warnings.push_back(
            "No supported five-fret guitar/bass notes");
    }
    result = std::move(loaded);
    return true;
} catch (const std::exception &exception) {
    result = {};
    error = exception.what();
    return false;
}
