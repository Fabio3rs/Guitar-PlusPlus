// Field-wise runtime-loader snapshot. Never dump padding or vector pointers.
#include "CMidiChart.h"
#include "CPlayer.h"
#include "ChartHopo.h"
#include <algorithm>
#include <cctype>
#include <cereal/external/rapidjson/ostreamwrapper.h>
#include <cereal/external/rapidjson/prettywriter.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <stdexcept>

namespace {
using Json = rapidjson::PrettyWriter<rapidjson::OStreamWrapper>;

void string(Json &json, const std::string &value) {
    json.String(value.data(), static_cast<rapidjson::SizeType>(value.size()));
}

void strings(Json &json, const std::vector<std::string> &values) {
    json.StartArray();
    for (const auto &value : values) {
        string(json, value);
    }
    json.EndArray();
}

void normalized(Json &json, const std::vector<CChart::Note> &notes,
                const std::vector<CChart::plusNote> &plus,
                const std::vector<CChart::Note> &bpm,
                const ChartSourceInfo &source) {
    json.StartObject();
    json.Key("ppq");
    json.Int(source.ppq);
    json.Key("offset");
    json.Double(source.offset);
    json.Key("hopo_policy");
    string(json, source.hopoPolicy);
    json.Key("hopo_threshold_ticks");
    json.Int(source.hopoThresholdTicks);
    json.Key("warnings");
    strings(json, source.warnings);
    double end = 0;
    size_t chords = 0;
    json.Key("notes");
    json.StartArray();
    for (size_t i = 0; i < notes.size(); ++i) {
        const auto &note = notes[i];
        end = std::max(end, note.time + note.lTime);
        chords += chart::isChord(note.type) ? 1 : 0;
        json.StartObject();
        json.Key("time");
        json.Double(note.time);
        json.Key("length");
        json.Double(note.lTime);
        json.Key("lanes");
        json.Int(note.type & chart::laneMask);
        json.Key("flags");
        json.Int(note.type);
        json.Key("tick");
        if (i < source.noteTicks.size()) {
            json.Int64(source.noteTicks[i]);
        } else {
            json.Null();
        }
        json.Key("hopo_reason");
        string(json, i < source.hopoReasons.size() ? source.hopoReasons[i]
                                                   : "feedback");
        json.EndObject();
    }
    json.EndArray();
    json.Key("chords");
    json.Uint64(chords);
    json.Key("duration");
    json.Double(end);
    json.Key("bpm");
    json.StartArray();
    for (const auto &entry : bpm) {
        json.StartArray();
        json.Double(entry.time);
        json.Double(entry.lTime);
        json.EndArray();
    }
    json.EndArray();
    json.Key("tempo_map");
    json.StartArray();
    for (const auto &entry : source.tempos) {
        json.StartArray();
        json.Int64(entry.tick);
        json.Double(entry.seconds);
        json.Double(entry.bpm);
        json.EndArray();
    }
    json.EndArray();
    json.Key("plus");
    json.StartArray();
    for (const auto &phrase : plus) {
        json.StartObject();
        json.Key("time");
        json.Double(phrase.time);
        json.Key("length");
        json.Double(phrase.lTime);
        json.Key("first");
        json.Uint64(phrase.firstNote);
        json.Key("last");
        json.Uint64(phrase.lastNote);
        json.EndObject();
    }
    json.EndArray();
    json.Key("gems");
    json.StartArray();
    for (const auto &gem : source.gems) {
        json.StartArray();
        json.Int64(gem.tick);
        json.Int64(gem.endTick);
        json.Int(gem.lane);
        json.Double(gem.seconds);
        json.Double(gem.duration);
        json.EndArray();
    }
    json.EndArray();
    json.Key("events");
    json.StartArray();
    for (const auto &event : source.events) {
        json.StartObject();
        json.Key("tick");
        json.Int64(event.tick);
        json.Key("time");
        json.Double(event.seconds);
        json.Key("track");
        string(json, event.track);
        json.Key("kind");
        string(json, event.kind);
        json.Key("text");
        string(json, event.text);
        json.EndObject();
    }
    json.EndArray();
    json.EndObject();
}

bool inspect(const std::string &path, const std::string &destination) {
    std::ofstream output(std::filesystem::u8path(destination), std::ios::binary);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    rapidjson::OStreamWrapper stream(output);
    Json json(stream);
    json.StartObject();
    json.Key("schema");
    json.Int(1);
    json.Key("path");
    string(json, path);
    auto extension = std::filesystem::u8path(path).extension().string();
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::string name, artist, charter, error;
    bool success = false;
    if (extension == ".mid" || extension == ".midi") {
        MidiChart midi;
        success = readMidiChart(std::filesystem::u8path(path), midi, error);
        name = midi.name;
        artist = midi.artist;
        charter = midi.charter;
        json.Key("tracks");
        strings(json, midi.trackNames);
        json.Key("warnings");
        strings(json, midi.source.warnings);
        json.Key("instruments");
        json.StartObject();
        for (const auto &entry : midi.instruments) {
            string(json, entry.first);
            normalized(json, entry.second.notes, entry.second.plus, midi.bpm,
                       entry.second.source);
        }
        json.EndObject();
    } else {
        const auto parts = CChart::availableInstruments(path);
        CChart::NotesData notes;
        CChart::Metadata metadata;
        json.Key("instruments");
        json.StartObject();
        for (const auto &part : parts) {
            const bool loaded = extension == ".chart"
                                    ? notes.loadFeedbackChartCPU(
                                          path, part, metadata, nullptr, true)
                                    : notes.loadChartCPU(path, part, metadata);
            if (loaded) {
                success = true;
                name = metadata.name;
                artist = metadata.artist;
                charter = metadata.charter;
                string(json, part);
                normalized(json, notes.gNotes, notes.gPlus, notes.BPM,
                           notes.source);
            }
        }
        json.EndObject();
        json.Key("fields");
        json.StartObject();
        for (const auto &field : metadata.fields) {
            string(json, field.first);
            string(json, field.second);
        }
        json.EndObject();
        if (!success) {
            error = "No valid playable instrument";
        }
    }
    json.Key("name");
    string(json, name);
    json.Key("artist");
    string(json, artist);
    json.Key("charter");
    string(json, charter);
    json.Key("success");
    json.Bool(success);
    json.Key("error");
    string(json, error);
    json.EndObject();
    output << '\n';
    return success;
}

void number(std::ostream &out, double value) {
    static_assert(sizeof(double) == sizeof(uint64_t));
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    out << std::hex << bits << std::dec;
}

void snapshot(std::ostream &out, const CPlayer &p, bool success) {
    const auto &n = p.Notes;
    out << "success " << success << "\nmetadata " << std::quoted(p.songName)
        << ' ' << std::quoted(p.songArtist) << ' ' << std::quoted(p.songCharter)
        << "\ninstrument " << std::quoted(p.instrument) << "\npositions "
        << n.notePos << ' ' << n.plusPos << ' ' << n.lastNotePicked << '\n';
    for (double v :
         {n.lastNoteEnd, n.BPMMinPosition, n.chartEnd, n.chartResolutionProp}) {
        number(out, v);
        out << ' ';
    }
    out << "\nfrets ";
    for (double v : n.fretsNotePickedTime) {
        number(out, v);
        out << ' ';
    }
    out << '\n';
    auto notes = [&](const char *name, const auto &items) {
        out << name << ' ' << items.size() << '\n';
        for (const auto &item : items) {
            number(out, item.time);
            out << ' ';
            number(out, item.lTime);
            out << ' ' << item.type << '\n';
        }
    };
    notes("notes", n.gNotes);
    notes("bpm", n.BPM);
    out << "plus " << n.gPlus.size() << '\n';
    for (const auto &item : n.gPlus) {
        number(out, item.time);
        out << ' ';
        number(out, item.lTime);
        out << ' ' << item.type << ' ' << item.firstNote << ' ' << item.lastNote
            << '\n';
    }
}
} // namespace

int run(int argc, char **argv) try {
    if (argc == 4 && std::string(argv[1]) == "inspect") {
        return inspect(argv[2], argv[3]) ? 0 : 1;
    }
    if (argc != 5) {
        std::cerr << "Usage: gpp_dump_chart feedback|compiled|midi INPUT "
                     "INSTRUMENT OUTPUT\n"
                     "       gpp_dump_chart inspect INPUT OUTPUT.json\n";
        return 2;
    }
    const std::string mode = argv[1];
    if (mode != "feedback" && mode != "compiled" && mode != "midi") {
        throw std::runtime_error("Unknown loader mode");
    }
    std::ofstream output(std::filesystem::u8path(argv[4]), std::ios::binary);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output.imbue(std::locale::classic());
    output << "gpp-runtime-chart-dump-v1 double=ieee754-bits\n";
    CPlayer player("chart evidence");
    player.instrument = argv[3];
    auto load = [&] {
        if (mode == "midi") {
            return player.Notes.loadMidiChart(argv[2], player);
        }
        return mode == "feedback"
                   ? player.Notes.loadFeedbackChart(argv[2], player)
                   : player.Notes.loadChart(argv[2], player);
    };
    output << "first\n";
    snapshot(output, player, load());
    output << "chartEndAfterQuery ";
    number(output, player.Notes.getChartEnd());
    output << "\nreload\n";
    snapshot(output, player, load());
    player.Notes.unloadChart();
    output << "unload\n";
    snapshot(output, player, false);
    output.close();
    return 0;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    std::vector<std::string> arguments;
    arguments.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        arguments.push_back(std::filesystem::path(argv[i]).u8string());
    }
    std::vector<char *> pointers;
    for (auto &argument : arguments) {
        pointers.push_back(argument.data());
    }
    return run(argc, pointers.data());
}
#else
int main(int argc, char **argv) { return run(argc, argv); }
#endif
