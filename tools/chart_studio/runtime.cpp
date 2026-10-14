#include "runtime.h"
#include "CChart.h"
#include "CMidiChart.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace ChartStudio {
Reference readReference(const fs::path &path, const std::string &instrument) {
    Reference result;
    result.instrument = instrument;
    result.name = path.filename().u8string();
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    std::vector<CChart::Note> notes;
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

void exportValidated(const State &state, const fs::path &path, bool replace) {
    for (const auto &input : {state.input, state.reference, state.audio}) {
        if (input.empty()) {
            continue;
        }
        std::error_code error;
        const bool same =
            fs::exists(path) && fs::equivalent(path, fs::u8path(input), error);
        if (same || fs::absolute(path).lexically_normal() ==
                        fs::absolute(fs::u8path(input)).lexically_normal()) {
            throw std::runtime_error(
                "Export cannot replace an input or reference file");
        }
    }
    const auto text = chartText(state);
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
    std::string difficulty = state.settings.difficulty;
    difficulty[0] = char(difficulty[0] - 'a' + 'A');
    CChart::NotesData loaded;
    CChart::Metadata metadata;
    if (!loaded.loadFeedbackChartCPU(temp.u8string(),
                                     "[" + difficulty + "Single]", metadata)) {
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
            std::abs(note.lTime - (seconds(entry.first + entry.second.second) -
                                   seconds(entry.first))) > 1e-6) {
            throw std::runtime_error(
                "Round-trip lanes/timing/sustain mismatch");
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
