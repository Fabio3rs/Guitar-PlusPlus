#include "core.h"
#include "plus_generator.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace ChartStudio {
int64_t exportDuration(const State &state, const Event &event) {
    if (state.settings.minimumSustain < 0) {
        // The default is strictly greater than half a quarter-note, slightly
        // larger than the Feedback HOPO window (quarter / 2.08). Compare
        // ticks directly: seconds rounding must not retain boundary notes.
        return static_cast<double>(event.duration) <=
                       -state.settings.minimumSustain * state.score.ppq
                   ? 0
                   : event.duration;
    }
    return event.note.duration < state.settings.minimumSustain ? 0
                                                               : event.duration;
}

double exportDurationSeconds(const State &state, const Event &event) {
    const auto duration = exportDuration(state, event);
    return audioSeconds(state, event.tick + duration) -
           audioSeconds(state, event.tick);
}

std::string singleChartText(const State &state) {
    validate(state);
    const auto errors = plusProblems(state);
    if (!errors.empty()) {
        throw std::runtime_error(errors.front());
    }
    std::map<int64_t, std::pair<int, int64_t>> attacks;
    for (const auto &event : state.events) {
        if (event.removed) {
            continue;
        }
        const auto duration = exportDuration(state, event);
        auto [it, inserted] = attacks.emplace(
            event.tick, std::make_pair(event.note.flags, duration));
        if (!inserted) {
            it->second.first |= event.note.flags;
            it->second.second = std::min(it->second.second, duration);
        }
    }
    if (attacks.empty()) {
        throw std::runtime_error("Cannot export an empty chart");
    }
    auto name = fs::u8path(state.input).stem().u8string();
    for (auto &c : name) {
        if (c == '"' || c == '\n' || c == '\r') {
            c = '_';
        }
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << "[Song]\n{\n  Name = \"" << name
        << "\"\n  Charter = \"Guitar++ Chart Studio\"\n  Resolution = "
        << state.score.ppq << "\n  Offset = " << state.settings.offset
        << "\n}\n[SyncTrack]\n{\n";
    for (const auto &tempo : state.score.tempos) {
        const int64_t milliBpm = exportMilliBpm(state, tempo);
        out << "  " << tempo.tick << " = B " << milliBpm << '\n';
    }
    for (const auto &signature : state.score.signatures) {
        int exponent = 0;
        for (int d = signature.denominator; d > 1; d >>= 1) {
            ++exponent;
        }
        out << "  " << signature.tick << " = TS " << signature.numerator << ' '
            << exponent << '\n';
    }
    out << "}\n" << partSection(state.settings) << "\n{\n";
    for (const auto &attack : attacks) {
        for (int lane = 0; lane < 5; ++lane) {
            if (attack.second.first & (1 << lane)) {
                out << "  " << attack.first << " = N " << lane << ' '
                    << attack.second.second << '\n';
            }
        }
    }
    auto phrases = state.plus.phrases;
    std::sort(phrases.begin(), phrases.end(),
              [](const auto &a, const auto &b) { return a.begin < b.begin; });
    // Keep N records before S records at shared ticks: the historical loader
    // otherwise merges a note into the special marker.
    for (const auto &p : phrases) {
        out << "  " << p.begin << " = S 2 " << p.end - p.begin << '\n';
    }
    out << "}\n";
    return out.str();
}
std::string chartText(const State &state) {
    validate(state);
    std::string result;
    for (const auto *part : parts(state)) {
        const auto selected = partState(state, part->partId);
        std::string text;
        try {
            text = singleChartText(selected);
        } catch (const std::exception &error) {
            throw std::runtime_error(partSection(part->settings) + ": " +
                                     error.what());
        }
        if (result.empty()) {
            result = text;
        } else {
            result +=
                text.substr(text.find(partSection(part->settings) + "\n"));
        }
    }
    return result;
}
} // namespace ChartStudio
