#include "runtime.h"
#include "CChart.h"
#include "CMidiChart.h"
#include "plus_generator.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace ChartStudio {
std::pair<PlusAnalysis, PlusDocument>
readPlusReference(const fs::path &path, const std::string &instrument) {
    CChart::NotesData data;
    CChart::Metadata metadata;
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    if (extension == ".mid" || extension == ".midi") {
        MidiChart midi;
        std::string error;
        if (!readMidiChart(path, midi, error)) {
            throw std::runtime_error(error);
        }
        const auto part = midi.instruments.find(instrument);
        if (part == midi.instruments.end()) {
            throw std::runtime_error("Reference instrument unavailable: " +
                                     instrument);
        }
        data.gNotes = part->second.notes;
        data.gPlus = part->second.plus;
        data.source = part->second.source;
        data.BPM = midi.bpm;
    } else {
        const bool loaded =
            extension == ".chart"
                ? data.loadFeedbackChartCPU(path.u8string(), instrument,
                                            metadata, nullptr, true)
                : data.loadChartCPU(path.u8string(), instrument, metadata);
        if (!loaded) {
            throw std::runtime_error("Cannot load Plus reference chart");
        }
    }
    State state;
    const bool originalTiming =
        data.source.ppq > 0 && !data.source.tempos.empty();
    if (originalTiming) {
        state.score.ppq = data.source.ppq;
        state.score.tempos.clear();
        state.score.signatures.clear();
        for (const auto &tempo : data.source.tempos) {
            state.score.tempos.push_back(
                {tempo.tick, int(std::llround(60000000.0 / tempo.bpm))});
        }
        state.settings.offset = data.source.offset;
        for (const auto &event : data.source.events) {
            if (event.kind == "time-signature") {
                int numerator = 0, exponent = 0;
                if (std::sscanf(event.text.c_str(), "%d/2^%d", &numerator,
                                &exponent) == 2 &&
                    numerator > 0 && exponent >= 0 && exponent <= 10) {
                    state.score.signatures.push_back(
                        {event.tick, numerator, 1 << exponent});
                }
            }
        }
    } else {
        state.score.ppq = 1000;
        state.score.tempos = {{0, 1000000}};
        state.settings.offset =
            data.gNotes.empty() ? 0 : std::min(0., data.gNotes.front().time);
    }
    size_t sourceIndex = 0;
    for (const auto &note : data.gNotes) {
        Event event;
        event.note.id = state.nextId++;
        event.tick =
            originalTiming && sourceIndex < data.source.noteTicks.size()
                ? data.source.noteTicks[sourceIndex]
                : audioTick(state, note.time);
        event.duration = std::max(
            int64_t(0), audioTick(state, note.time + note.lTime) - event.tick);
        ++sourceIndex;
        // Open attacks remain attacks; no export or mapping is performed here.
        event.note.flags = (note.type & 31) ? note.type & 31 : 1;
        state.events.push_back(event);
    }
    state.settings.minimumSustain = 0;
    synchronize(state);
    auto analysis = analyzePlus(state);
    if (!originalTiming) {
        analysis.diagnostics.push_back(
            "Original tick/meter evidence unavailable; reference timeline "
            "reconstructed at 1 ms. Musical boundaries are not authoritative.");
    }
    for (size_t i = 0; i < analysis.attacks.size() && i < data.gNotes.size();
         ++i) {
        analysis.attacks[i].mask = data.gNotes[i].type & (31 | nf_open);
    }
    PlusDocument document;
    for (const auto &p : data.gPlus) {
        if (p.firstNote >= data.gNotes.size() ||
            p.lastNote >= data.gNotes.size() || p.firstNote > p.lastNote) {
            analysis.diagnostics.push_back(
                "Invalid runtime Plus indices; phrase omitted from simulation");
            continue;
        }
        PlusPhrase phrase;
        phrase.id = document.nextId++;
        // Preserve actual membership returned by the loader, including its
        // cursor semantics.
        phrase.begin = audioTick(state, p.time);
        phrase.end = audioTick(state, p.time + p.lTime);
        phrase.runtimeFirst = int64_t(p.firstNote);
        phrase.runtimeLast = int64_t(p.lastNote);
        phrase.reason =
            "Reference phrase; actual loader indices used for simulation";
        document.phrases.push_back(phrase);
    }
    return {std::move(analysis), std::move(document)};
}

Reference readReference(const fs::path &path, const std::string &instrument) {
    Reference result;
    result.instrument = instrument;
    result.name = path.filename().u8string();
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    std::vector<CChart::Note> notes;
    if (extension == ".gppchartproj") {
        const auto project = loadProject(path);
        result = asReference(partState(project, findPart(project, instrument)));
        result.name = path.filename().u8string();
        result.instrument = instrument;
        return result;
    }
    if (extension == ".mid" || extension == ".midi") {
        MidiChart chart;
        std::string error;
        if (!readMidiChart(path, chart, error)) {
            throw std::runtime_error(error);
        }
        const auto found = chart.instruments.find(instrument);
        if (found == chart.instruments.end()) {
            throw std::runtime_error("Reference instrument unavailable: " +
                                     instrument);
        }
        notes = found->second.notes;
    } else {
        CChart::NotesData data;
        CChart::Metadata metadata;
        const bool valid =
            extension == ".chart"
                ? data.loadFeedbackChartCPU(path.u8string(), instrument,
                                            metadata)
                : data.loadChartCPU(path.u8string(), instrument, metadata);
        if (!valid) {
            throw std::runtime_error("Cannot load reference chart/instrument");
        }
        notes = std::move(data.gNotes);
    }
    uint64_t id = 1;
    for (const auto &note : notes) {
        result.notes.push_back(
            {id++, note.time, note.lTime, note.type & (31 | nf_open)});
    }
    return result;
}

Reference asReference(const State &state) {
    Reference result;
    for (const auto &event : state.events) {
        if (!event.removed) {
            auto note = event.note;
            note.duration = exportDurationSeconds(state, event);
            result.notes.push_back(note);
        }
    }
    return result;
}

void exportValidated(const State &project, const fs::path &path, bool replace) {
    // Protect references belonging to every part, not only the active one.
    std::vector<fs::path> sources{fs::u8path(project.input),
                                  fs::u8path(project.audio)};
    for (const auto *part : parts(project)) {
        sources.push_back(fs::u8path(part->reference));
    }
    protectDestination(path, sources);
    const auto text = chartText(project);
    const auto temp =
        fs::temp_directory_path() /
        ("gpp-chart-check-" + hash(text) + "-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".chart");
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code e;
            fs::remove(path, e);
        }
    } cleanup{temp};
    safeWrite(temp, text, false);
    for (const auto *part : parts(project)) {
        try {
            const auto selected = partState(project, part->partId);
            const State &state = selected;
            CChart::NotesData loaded;
            CChart::Metadata metadata;
            if (!loaded.loadFeedbackChartCPU(
                    temp.u8string(), partSection(state.settings), metadata)) {
                throw std::runtime_error("Runtime rejected exported chart");
            }
            std::map<int64_t, std::pair<int, int64_t>> expected;
            for (const auto &event : state.events) {
                if (event.removed) {
                    continue;
                }
                const auto duration = exportDuration(state, event);
                auto [it, inserted] = expected.emplace(
                    event.tick, std::make_pair(event.note.flags, duration));
                if (!inserted) {
                    it->second.first |= event.note.flags;
                    it->second.second = std::min(it->second.second, duration);
                }
            }
            if (expected.size() != loaded.gNotes.size()) {
                throw std::runtime_error("Round-trip attack count mismatch");
            }
            auto seconds = [&](int64_t tick) {
                double time = state.settings.offset;
                int64_t previous = 0, bpm = 120000;
                for (const auto &tempo : state.score.tempos) {
                    if (tempo.tick > tick) {
                        break;
                    }
                    time += double(tempo.tick - previous) * 60000.0 /
                            (state.score.ppq * double(bpm));
                    previous = tempo.tick;
                    bpm = exportMilliBpm(state, tempo);
                }
                return time + double(tick - previous) * 60000.0 /
                                  (state.score.ppq * double(bpm));
            };
            size_t index = 0;
            for (const auto &entry : expected) {
                const auto &note = loaded.gNotes[index++];
                if ((note.type & 31) != entry.second.first ||
                    std::abs(note.time - seconds(entry.first)) > 1e-6 ||
                    std::abs(note.lTime -
                             (seconds(entry.first + entry.second.second) -
                              seconds(entry.first))) > 1e-6) {
                    throw std::runtime_error(
                        "Round-trip lanes/timing/sustain mismatch");
                }
            }
            auto phrases = state.plus.phrases;
            std::sort(
                phrases.begin(), phrases.end(),
                [](const auto &a, const auto &b) { return a.begin < b.begin; });
            const auto attacks = plusAttacks(state);
            if (phrases.size() != loaded.gPlus.size()) {
                throw std::runtime_error("Round-trip Plus count mismatch");
            }
            for (size_t i = 0; i < phrases.size(); ++i) {
                const auto &p = phrases[i];
                const auto &actual = loaded.gPlus[i];
                const auto range = plusAttackRange(attacks, p);
                if (range.first == range.second ||
                    actual.firstNote != range.first ||
                    actual.lastNote != range.second - 1 ||
                    std::abs(actual.time - seconds(p.begin)) > 1e-6 ||
                    std::abs(actual.lTime - (seconds(exportPlusEnd(state, p)) -
                                             seconds(p.begin))) > 1e-6) {
                    throw std::runtime_error(
                        "Round-trip Plus boundaries or indices mismatch at "
                        "phrase " +
                        std::to_string(p.id) + " (ticks " +
                        std::to_string(p.begin) + ".." + std::to_string(p.end) +
                        ", expected attacks " + std::to_string(range.first) +
                        ".." + std::to_string(range.second - 1) + ", loaded " +
                        std::to_string(actual.firstNote) + ".." +
                        std::to_string(actual.lastNote) + ")");
                }
            }
        } catch (const std::exception &error) {
            throw std::runtime_error(partSection(part->settings) + ": " +
                                     error.what());
        }
    }
    safeWrite(path, text, replace);
}

std::string compare(const Reference &left, const Reference &right,
                    double leftOffset, double rightOffset, double window) {
    if (!std::isfinite(leftOffset) || !std::isfinite(rightOffset) ||
        !std::isfinite(window) || window < .001 ||
        std::abs(leftOffset) > 36000 || std::abs(rightOffset) > 36000) {
        throw std::runtime_error("Invalid comparison timing");
    }
    struct Bin {
        int attacks = 0, chords = 0, sustains = 0;
        double duration = 0;
        std::map<int, int> shapes;
    };
    std::map<int64_t, std::array<Bin, 2>> bins;
    const Reference *sources[] = {&left, &right};
    const double offsets[] = {leftOffset, rightOffset};
    for (int s = 0; s < 2; ++s) {
        for (const auto &note : sources[s]->notes) {
            auto &bin =
                bins[int64_t(std::floor((note.time + offsets[s]) / window))][s];
            ++bin.attacks;
            bin.chords += (note.flags & (note.flags - 1)) != 0;
            bin.sustains += note.duration > 0;
            bin.duration += note.duration;
            ++bin.shapes[note.flags];
        }
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "{\"schema\":1,\"window_seconds\":" << window << ",\"windows\":[";
    bool first = true;
    for (const auto &entry : bins) {
        if (!first) {
            out << ',';
        }
        first = false;
        out << "{\"start\":" << static_cast<double>(entry.first) * window
            << ",\"charts\":[";
        for (int s = 0; s < 2; ++s) {
            if (s) {
                out << ',';
            }
            const auto &b = entry.second[s];
            out << "{\"attacks\":" << b.attacks << ",\"chords\":" << b.chords
                << ",\"sustains\":" << b.sustains
                << ",\"sustain_seconds\":" << b.duration << ",\"shapes\":{";
            bool firstShape = true;
            for (const auto &shape : b.shapes) {
                if (!firstShape) {
                    out << ',';
                }
                firstShape = false;
                out << '"' << shape.first << "\":" << shape.second;
            }
            out << "}}";
        }
        out << "]}";
    }
    out << "],\"interpretation\":\"Arrangement comparison; lane equality is "
           "not a quality score.\"}\n";
    return out.str();
}
} // namespace ChartStudio
