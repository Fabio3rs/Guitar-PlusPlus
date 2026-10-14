#include "core.h"
#include "plus_generator.h"
#include "plus_simulation.h"
#include <algorithm>
#include <stdexcept>

namespace ChartStudio {
namespace {
const std::map<std::string, std::string> instruments{{"guitar", "Single"},
                                                     {"bass", "DoubleBass"},
                                                     {"rhythm", "DoubleRhythm"},
                                                     {"coop", "DoubleGuitar"},
                                                     {"keys", "Keyboard"}};
}

std::string partSection(const Settings &settings) {
    const auto found = instruments.find(settings.instrument);
    if (found == instruments.end()) {
        throw std::runtime_error("Unsupported five-lane instrument: " +
                                 settings.instrument);
    }
    auto difficulty = settings.difficulty;
    if (difficulty != "expert" && difficulty != "hard" &&
        difficulty != "medium" && difficulty != "easy") {
        throw std::runtime_error("Invalid part difficulty");
    }
    difficulty[0] = char(difficulty[0] - 'a' + 'A');
    return "[" + difficulty + found->second + "]";
}

void setPartSection(Settings &settings, const std::string &section) {
    const auto name = !section.empty() && section.front() == '['
                          ? section
                          : "[" + section + "]";
    for (const auto &instrument : instruments) {
        for (const auto *difficulty : {"expert", "hard", "medium", "easy"}) {
            auto candidate = settings;
            candidate.instrument = instrument.first;
            candidate.difficulty = difficulty;
            if (partSection(candidate) == name) {
                settings = std::move(candidate);
                return;
            }
        }
    }
    throw std::runtime_error("Unsupported part section: " + section +
                             "; use e.g. ExpertSingle or ExpertDoubleBass");
}

Settings parsePart(const std::string &association, const Settings &defaults) {
    std::vector<std::string> fields;
    std::istringstream stream(association);
    std::string field;
    while (std::getline(stream, field, ':')) {
        fields.push_back(field);
    }
    if (fields.size() < 2 || fields.size() > 4 || association.back() == ':') {
        throw std::runtime_error(
            "Part requires SECTION:TRACK[:CHANNEL[:PROGRAM]]");
    }
    auto settings = defaults;
    setPartSection(settings, fields[0]);
    int *targets[] = {&settings.track, &settings.channel, &settings.program};
    settings.channel = settings.program = -1;
    for (size_t i = 1; i < fields.size(); ++i) {
        size_t used = 0;
        const auto value = std::stoll(fields[i], &used);
        if (used != fields[i].size() || value < (i == 1 ? 0 : -1) ||
            value > INT32_MAX) {
            throw std::runtime_error("Invalid part source: " + association);
        }
        *targets[i - 1] = int(value);
    }
    settings.validate();
    return settings;
}

std::vector<const Part *> parts(const State &state) {
    std::vector<const Part *> result{&state};
    for (const auto &part : state.otherParts) {
        result.push_back(&part);
    }
    std::sort(result.begin(), result.end(), [](const auto *a, const auto *b) {
        return partSection(a->settings) < partSection(b->settings);
    });
    return result;
}

void activatePart(State &state, uint64_t id) {
    if (state.partId == id) {
        return;
    }
    for (auto &part : state.otherParts) {
        if (part.partId == id) {
            const auto offset = state.settings.offset;
            std::swap(static_cast<Part &>(state), part);
            state.settings.offset = offset;
            part.settings.offset = offset;
            return;
        }
    }
    throw std::runtime_error("Part not found");
}

State partState(const State &state, uint64_t id) {
    const Part *selected = nullptr;
    for (const auto *part : parts(state)) {
        if (part->partId == id) {
            selected = part;
            break;
        }
    }
    if (!selected) {
        throw std::runtime_error("Part not found");
    }
    State result;
    static_cast<Part &>(result) = *selected;
    result.score = state.score;
    result.alignment = state.alignment;
    result.input = state.input;
    result.inputHash = state.inputHash;
    result.audio = state.audio;
    result.markers = state.markers;
    result.nextPartId = state.nextPartId;
    return result;
}

uint64_t findPart(const State &state, const std::string &section) {
    Settings target;
    setPartSection(target, section);
    for (const auto *part : parts(state)) {
        if (partSection(part->settings) == partSection(target)) {
            return part->partId;
        }
    }
    throw std::runtime_error("Project part unavailable: " + section);
}

State generateParts(State state, const std::vector<Pattern> &library,
                    const std::map<uint64_t, Projection> &projections,
                    const PartProgress &progress) {
    validate(state);
    if (!state.input.empty() && fs::exists(fs::u8path(state.input)) &&
        fileHash(fs::u8path(state.input)) != state.inputHash) {
        throw std::runtime_error(
            "Source MIDI changed; reopen it before regeneration");
    }
    const auto active = state.partId;
    std::vector<uint64_t> order;
    for (const auto *part : parts(state)) {
        order.push_back(part->partId);
    }
    for (size_t i = 0; i < order.size(); ++i) {
        activatePart(state, order[i]);
        const auto section = partSection(state.settings);
        if (progress) {
            progress(i, order.size(), section);
        }
        try {
            const auto projection = projections.find(state.partId);
            generate(state, library, {},
                     projection == projections.end() ? nullptr
                                                     : &projection->second);
            if (state.plus.settings.enabled) {
                state.plus = proposePlus(state, state.plus.settings).document;
            }
        } catch (const std::exception &e) {
            throw std::runtime_error(section + ": " + e.what());
        }
    }
    activatePart(state, active);
    if (progress) {
        progress(order.size(), order.size(), "Finished");
    }
    validate(state);
    return state;
}

std::string partsReport(const State &state, bool plus, int trials,
                        uint64_t seed) {
    if (state.otherParts.empty()) {
        return plus ? plusReport(state, trials, seed) : report(state);
    }
    std::ostringstream out;
    out << "{\"schema\":2,\"parts\":[";
    bool first = true;
    for (const auto *part : parts(state)) {
        if (!first) {
            out << ',';
        }
        first = false;
        const auto selected = partState(state, part->partId);
        out << "{\"section\":\"" << partSection(part->settings)
            << "\",\"track\":" << part->settings.track
            << ",\"channel\":" << part->settings.channel
            << ",\"program\":" << part->settings.program << ",\"report\":"
            << (plus ? plusReport(selected, trials, seed) : report(selected))
            << '}';
    }
    out << "]}\n";
    return out.str();
}
} // namespace ChartStudio
