#include "audio/audio.h"
#include "core.h"
#include "runtime.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv) try {
    using namespace ChartStudio;
    if (argc < 2 || std::string(argv[1]) == "--help") {
        std::cout
            << "gpp_score_to_chart inspect MIDI\n"
               "gpp_score_to_chart fft AUDIO --at SECONDS [--output "
               "FILE.json]\n"
               "gpp_score_to_chart convert MIDI --track N --output FILE.chart "
               "[--project FILE.gppchartproj]\n"
               "  [--difficulty expert|hard|medium|easy] [--algorithm "
               "beam|heuristic] [--channel 0..15] [--program 0..127]\n"
               "  [--offset-ms N] [--max-chord 1..3] [--density 0..1] "
               "[--minimum-sustain auto|SECONDS] [--sustain-beats BEATS]\n"
               "  [--cache LIBRARY.sqlite] [--no-structural] [--no-cache] "
               "[--replace] [--report FILE.json]\n"
               "  [--weight0 N .. --weight5 N] [--begin-tick N --end-tick N]\n"
               "gpp_score_to_chart compare LEFT RIGHT [--instrument "
               "'[ExpertSingle]'] [--reference-instrument PART]\n"
               "  [--left-offset-ms N] [--right-offset-ms N] [--output "
               "FILE.json] [--replace]\n"
               "gpp_score_to_chart patterns PROJECT "
               "list|confirm|lock|unlock|recalculate|promote|clear-"
               "experimental\n"
               "  [--event ID] [--cache LIBRARY.sqlite]\n"
               "Track/channel/program indices are zero-based. Source files are "
               "never overwritten.\n";
        return 0;
    }
    const std::string command = argv[1];
    std::vector<std::string> positional;
    std::map<std::string, std::string> options;
    const std::set<std::string> switches{"--replace", "--no-cache",
                                         "--no-structural"};
    const std::set<std::string> values{"--at",
                                       "--track",
                                       "--output",
                                       "--project",
                                       "--difficulty",
                                       "--algorithm",
                                       "--channel",
                                       "--program",
                                       "--offset-ms",
                                       "--max-chord",
                                       "--density",
                                       "--minimum-sustain",
                                       "--sustain-beats",
                                       "--cache",
                                       "--report",
                                       "--begin-tick",
                                       "--end-tick",
                                       "--instrument",
                                       "--reference-instrument",
                                       "--left-offset-ms",
                                       "--right-offset-ms",
                                       "--event",
                                       "--weight0",
                                       "--weight1",
                                       "--weight2",
                                       "--weight3",
                                       "--weight4",
                                       "--weight5"};
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (switches.count(arg)) {
            options[arg] = "1";
        } else if (values.count(arg)) {
            if (++i == argc) {
                throw std::runtime_error("Missing value: " + arg);
            }
            options[arg] = argv[i];
        } else if (arg.rfind("--", 0) == 0) {
            throw std::runtime_error("Unknown option " + arg);
        } else {
            positional.push_back(arg);
        }
    }
    auto get = [&](const std::string &key, const std::string &fallback = "") {
        const auto it = options.find(key);
        return it == options.end() ? fallback : it->second;
    };
    auto number = [&](const std::string &key, double fallback = 0) {
        if (!options.count(key)) {
            return fallback;
        }
        size_t used = 0;
        const double value = std::stod(options.at(key), &used);
        if (used != options.at(key).size() || !std::isfinite(value)) {
            throw std::runtime_error("Invalid number " + key);
        }
        return value;
    };
    auto integer = [&](const std::string &key,
                       int64_t fallback = 0) -> int64_t {
        if (!options.count(key)) {
            return fallback;
        }
        size_t used = 0;
        const auto value = std::stoll(options.at(key), &used);
        if (used != options.at(key).size() || value < -1 || value > INT32_MAX) {
            throw std::runtime_error("Invalid integer " + key);
        }
        return value;
    };
    if (positional.empty()) {
        throw std::runtime_error("Missing input path");
    }
    if (command == "fft") {
        gpp::audio::DeviceSettings device;
        device.device = 0;
        gpp::audio::BassSystem audio(device);
        const auto result =
            gpp::audio::analyze(fs::u8path(positional[0]), number("--at"), {});
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << "{\"fft_size\":8192,\"sample_rate\":" << result.rate
            << ",\"position_seconds\":" << number("--at")
            << ",\"interpretation\":\"Mixed audio harmonic candidates, not "
               "instrument-separated notes or a replacement tempo "
               "map\",\"candidates\":[";
        bool first = true;
        for (const auto &note : result.notes) {
            if (!first) {
                out << ',';
            }
            first = false;
            out << "{\"pitch\":" << note.midi << ",\"hz\":" << note.frequency
                << ",\"score\":" << note.score << '}';
        }
        out << "],\"spectrum\":[";
        first = true;
        for (float value : result.spectrum) {
            if (!first) {
                out << ',';
            }
            first = false;
            out << value;
        }
        out << "]}\n";
        if (options.count("--output")) {
            protectDestination(fs::u8path(get("--output")),
                               {fs::u8path(positional[0])});
            safeWrite(fs::u8path(get("--output")), out.str(),
                      options.count("--replace"));
        } else {
            std::cout << out.str();
        }
        return 0;
    }
    if (command == "inspect") {
        std::cout << inspect(importMidi(fs::u8path(positional[0])));
        return 0;
    }
    if (command == "compare") {
        if (positional.size() != 2) {
            throw std::runtime_error("Compare needs two reference paths");
        }
        const auto output =
            compare(readReference(fs::u8path(positional[0]),
                                  get("--instrument", "[ExpertSingle]")),
                    readReference(fs::u8path(positional[1]),
                                  get("--reference-instrument",
                                      get("--instrument", "[ExpertSingle]"))),
                    number("--left-offset-ms") / 1000,
                    number("--right-offset-ms") / 1000);
        if (options.count("--output")) {
            protectDestination(
                fs::u8path(get("--output")),
                {fs::u8path(positional[0]), fs::u8path(positional[1])});
            safeWrite(fs::u8path(get("--output")), output,
                      options.count("--replace"));
        } else {
            std::cout << output;
        }
        return 0;
    }
    Session session;
    if (command == "patterns") {
        if (positional.size() != 2) {
            throw std::runtime_error("Patterns needs project and action");
        }
        session.openProject(fs::u8path(positional[0]));
        const auto &action = positional[1];
        if (action == "list") {
            for (const auto &entry : session.state.patterns) {
                std::cout << hash(entry.first)
                          << " status=" << entry.second.status << " lanes=";
                for (int mask : entry.second.masks) {
                    std::cout << mask << ' ';
                }
                std::cout << '\n';
            }
            return 0;
        }
        const auto id = uint64_t(integer("--event"));
        if (action == "clear-experimental") {
            session.clearExperimental();
        } else if (action == "confirm" || action == "lock" ||
                   action == "unlock") {
            session.pattern(id,
                            action == "lock"      ? 2
                            : action == "confirm" ? 1
                                                  : 0,
                            true);
        } else if (action == "recalculate") {
            session.recalculate(id);
        } else if (action == "promote") {
            if (!options.count("--cache")) {
                throw std::runtime_error("Promotion requires --cache");
            }
            bool found = false;
            for (const auto &event : session.state.events) {
                if (event.note.id == id) {
                    promote(fs::u8path(get("--cache")),
                            session.state.patterns.at(event.exact));
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::runtime_error("Event ID unavailable");
            }
            return 0;
        } else {
            throw std::runtime_error("Unknown patterns action");
        }
        session.save(session.project);
        return 0;
    }
    if (command != "convert") {
        throw std::runtime_error("Unknown command");
    }
    if (!options.count("--output")) {
        throw std::runtime_error("Conversion requires --output");
    }
    const auto output = fs::u8path(get("--output"));
    auto project = options.count("--project")
                       ? fs::u8path(get("--project"))
                       : fs::path(output).replace_extension(".gppchartproj");
    const auto source = fs::u8path(positional[0]);
    const auto library = fs::u8path(get("--cache"));
    const auto reportPath = fs::u8path(get("--report"));
    protectDestination(output, {source, project, library, reportPath});
    protectDestination(project, {source, library, reportPath});
    if (!reportPath.empty()) {
        protectDestination(reportPath, {source, library});
    }
    if (fs::exists(project)) {
        session.openProject(project);
        if (fileHash(fs::u8path(positional[0])) != session.state.inputHash) {
            throw std::runtime_error(
                "Existing project belongs to another MIDI");
        }
    } else {
        session.openMidi(fs::u8path(positional[0]));
    }
    auto &s = session.state.settings;
    s.track = int(integer("--track", s.track));
    s.channel = int(integer("--channel", s.channel));
    s.program = int(integer("--program", s.program));
    s.difficulty = get("--difficulty", s.difficulty);
    s.algorithm = get("--algorithm", s.algorithm);
    s.offset = number("--offset-ms", s.offset * 1000) / 1000;
    s.maxChord = int(integer("--max-chord", s.maxChord));
    s.density = number("--density", s.density);
    s.minimumSustain = get("--minimum-sustain") == "auto"
                           ? -.5
                           : number("--minimum-sustain", s.minimumSustain);
    if (options.count("--minimum-sustain") &&
        get("--minimum-sustain") != "auto" && s.minimumSustain < 0) {
        throw std::runtime_error("Minimum sustain seconds must be nonnegative; "
                                 "use --sustain-beats for musical limits");
    }
    if (options.count("--sustain-beats")) {
        if (options.count("--minimum-sustain")) {
            throw std::runtime_error(
                "Choose beats or seconds for sustain cutoff, not both");
        }
        const double beats = number("--sustain-beats");
        if (beats <= 0 || beats > 128) {
            throw std::runtime_error(
                "Sustain beats must be greater than zero and at most 128");
        }
        s.minimumSustain = -beats;
    }
    s.reuse = !options.count("--no-cache");
    s.structural = !options.count("--no-structural");
    for (size_t i = 0; i < s.weights.size(); ++i) {
        s.weights[i] =
            int(integer("--weight" + std::to_string(i), s.weights[i]));
    }
    std::optional<std::pair<int64_t, int64_t>> region;
    if (options.count("--begin-tick") || options.count("--end-tick")) {
        if (!options.count("--begin-tick") || !options.count("--end-tick")) {
            throw std::runtime_error("Region needs both boundaries");
        }
        region = {
            {int64_t(integer("--begin-tick")), int64_t(integer("--end-tick"))}};
        if (region->first < 0 || region->second <= region->first) {
            throw std::runtime_error("Invalid region");
        }
    }
    session.run(loadLibrary(fs::u8path(get("--cache"))), region);
    exportValidated(session.state, output, options.count("--replace"));
    session.save(project);
    const auto summary = report(session.state);
    if (options.count("--report")) {
        safeWrite(fs::u8path(get("--report")), summary,
                  options.count("--replace"));
    }
    std::cout << summary;
    for (const auto &conflict : session.state.conflicts) {
        std::cerr << "Conflict: " << conflict << '\n';
    }
    return session.state.conflicts.empty() ? 0 : 3;
} catch (const std::exception &error) {
    std::cerr << "Chart Studio: " << error.what() << '\n';
    return 1;
}
