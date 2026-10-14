#include "alignment.h"
#include "audio/audio.h"
#include "core.h"
#include "plus_simulation.h"
#include "runtime.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv) try {
    using namespace ChartStudio;
    if (argc < 2 || std::string(argv[1]) == "--help") {
        std::cout << "Plus: convert MIDI --track N --plus auto --plus-profile "
                     "strategic --output FILE.chart\n"
                     "  [--plus-reserve 0.4 --plus-spacing 8 --plus-bars 1]\n"
                     "  [--plus-structure 2 --plus-strategy 4 --plus-risk 2 "
                     "--plus-trials 256 --plus-seed 42]\n"
                     "  plus-generate PROJECT --output FILE.chart [--project "
                     "NEW_PROJECT] [--report FILE.json]\n"
                     "  plus-analyze CHART|PROJECT [--output FILE.json]\n"
                     "  plus-simulate CHART|PROJECT [--output FILE.json] "
                     "[--instrument PART]\n";
        std::cout
            << "gpp_score_to_chart align MIDI --audio AUDIO [--align "
               "offset|linear|auto] --output FILE.json\n"
               "  [--align-min-offset SEC --align-max-offset SEC]\n"
               "  convert: [--audio AUDIO --align none|offset|linear|auto] "
               "[--alignment FILE.json]\n"
               "  [--alignment-candidate 0..2] (explicit acceptance of an "
               "ambiguous candidate)\n"
               "gpp_score_to_chart inspect MIDI\n"
               "gpp_score_to_chart features MIDI --track N [--output "
               "FILE.json]\n"
               "gpp_score_to_chart events PROJECT [--output FILE.json]\n"
               "gpp_score_to_chart fft AUDIO --at SECONDS [--output "
               "FILE.json]\n"
               "gpp_score_to_chart convert MIDI --track N --output FILE.chart "
               "[--project FILE.gppchartproj]\n"
               "  [--difficulty expert|hard|medium|easy] [--algorithm "
               "beam|heuristic|umap] [--projection FILE.json] [--channel "
               "0..15] [--program 0..127]\n"
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
    const std::set<std::string> values{"--plus",
                                       "--plus-profile",
                                       "--plus-reserve",
                                       "--plus-spacing",
                                       "--plus-bars",
                                       "--plus-structure",
                                       "--plus-strategy",
                                       "--plus-risk",
                                       "--plus-trials",
                                       "--plus-seed",
                                       "--at",
                                       "--audio",
                                       "--align",
                                       "--alignment",
                                       "--alignment-candidate",
                                       "--align-min-offset",
                                       "--align-max-offset",
                                       "--projection",
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
    auto plusSettings = [&](const PlusSettings &saved) {
        auto settings = options.count("--plus-profile")
                            ? plusProfile(get("--plus-profile"))
                            : saved;
        settings.enabled = true;
        settings.reserve = number("--plus-reserve", settings.reserve);
        settings.spacingBars = number("--plus-spacing", settings.spacingBars);
        settings.preferredBars =
            int(integer("--plus-bars", settings.preferredBars));
        settings.structureWeight =
            number("--plus-structure", settings.structureWeight);
        settings.strategyWeight =
            number("--plus-strategy", settings.strategyWeight);
        settings.riskWeight = number("--plus-risk", settings.riskWeight);
        settings.validate();
        return settings;
    };
    const int plusTrials = int(integer("--plus-trials", 256));
    const auto plusSeed = integer("--plus-seed", 42);
    if (plusTrials < 1 || plusTrials > 4096 || plusSeed < 0) {
        throw std::runtime_error("Invalid Plus trials or seed");
    }
    if (options.count("--plus") && get("--plus") != "auto" &&
        get("--plus") != "none") {
        throw std::runtime_error("--plus requires auto or none");
    }
    if (command == "plus-analyze" || command == "plus-simulate" ||
        command == "plus-generate") {
        const auto input = fs::u8path(positional[0]);
        std::string json;
        if (command == "plus-generate") {
            if (!options.count("--output")) {
                throw std::runtime_error("Plus generation requires --output");
            }
            Session session;
            session.openProject(input);
            std::optional<std::pair<int64_t, int64_t>> region;
            if (options.count("--begin-tick") || options.count("--end-tick")) {
                if (!options.count("--begin-tick") ||
                    !options.count("--end-tick")) {
                    throw std::runtime_error("Both region boundaries required");
                }
                region = {{integer("--begin-tick"), integer("--end-tick")}};
            }
            const auto proposal =
                proposePlus(session.state,
                            plusSettings(session.state.plus.settings), region);
            session.applyPlus(proposal);
            const auto output = fs::u8path(get("--output"));
            auto project = fs::u8path(
                get("--project", output.u8string() + ".gppchartproj"));
            protectDestination(output, {input, project});
            const std::vector<fs::path> sources{
                input, fs::u8path(session.state.input),
                fs::u8path(session.state.audio),
                fs::u8path(session.state.reference), output};
            protectDestination(project, sources);
            if (!options.count("--replace")) {
                for (const auto &destination :
                     {output, project, fs::u8path(get("--report"))}) {
                    if (!destination.empty() && fs::exists(destination)) {
                        throw std::runtime_error(
                            "Destination exists; use --replace: " +
                            destination.u8string());
                    }
                }
            }
            if (options.count("--report")) {
                protectDestination(fs::u8path(get("--report")), sources);
                protectDestination(fs::u8path(get("--report")), {project});
            }
            exportValidated(session.state, output, options.count("--replace"));
            saveProject(project, session.state, options.count("--replace"));
            json = plusReport(session.state, plusTrials, uint64_t(plusSeed));
            for (const auto &message : proposal.diagnostics) {
                std::cerr << "Plus: " << message << '\n';
            }
            if (options.count("--report")) {
                safeWrite(fs::u8path(get("--report")), json,
                          options.count("--replace"));
            }
        } else {
            if (input.extension() == ".gppchartproj") {
                json = plusReport(loadProject(input), plusTrials,
                                  uint64_t(plusSeed));
            } else {
                const auto reference = readPlusReference(
                    input, get("--instrument", "[ExpertSingle]"));
                json = plusReport(reference.first, reference.second, plusTrials,
                                  uint64_t(plusSeed));
            }
            if (options.count("--output")) {
                protectDestination(fs::u8path(get("--output")), {input});
                safeWrite(fs::u8path(get("--output")), json,
                          options.count("--replace"));
            }
        }
        std::cout << json;
        return 0;
    }
    auto alignmentSettings = [&] {
        AlignmentSettings settings;
        settings.mode = get("--align", "auto");
        settings.minimumOffset = number("--align-min-offset", -15);
        settings.maximumOffset = number("--align-max-offset", 30);
        settings.validate();
        return settings;
    };
    if (command == "align") {
        if (!options.count("--audio")) {
            throw std::runtime_error("Alignment requires --audio");
        }
        Session source;
        source.openMidi(fs::u8path(positional[0]));
        gpp::audio::DeviceSettings device;
        device.device = 0;
        gpp::audio::BassSystem audio(device);
        const auto result = analyzeAlignment(
            source.state, fs::u8path(get("--audio")), alignmentSettings());
        const auto json = alignmentJson(result);
        if (options.count("--output")) {
            protectDestination(
                fs::u8path(get("--output")),
                {fs::u8path(positional[0]), fs::u8path(get("--audio"))});
            safeWrite(fs::u8path(get("--output")), json,
                      options.count("--replace"));
        }
        std::cout << json << '\n';
        return 0; // Analysis reports uncertainty; it never applies a candidate.
    }
    if (command == "features" || command == "events") {
        Session session;
        if (command == "features") {
            session.openMidi(fs::u8path(positional[0]));
            auto &settings = session.state.settings;
            settings.track = int(integer("--track", -1));
            settings.channel = int(integer("--channel", -1));
            settings.program = int(integer("--program", -1));
            settings.difficulty = get("--difficulty", "expert");
            session.run();
        } else {
            session.openProject(fs::u8path(positional[0]));
        }
        const auto output = experimentEvents(session.state);
        if (options.count("--output")) {
            protectDestination(fs::u8path(get("--output")),
                               {fs::u8path(positional[0]),
                                fs::u8path(session.state.input),
                                fs::u8path(session.state.audio),
                                fs::u8path(session.state.reference)});
            safeWrite(fs::u8path(get("--output")), output,
                      options.count("--replace"));
        } else {
            std::cout << output;
        }
        return 0;
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
    protectDestination(output, {source, project, library, reportPath,
                                fs::u8path(get("--audio")),
                                fs::u8path(get("--alignment"))});
    protectDestination(project,
                       {source, library, reportPath, fs::u8path(get("--audio")),
                        fs::u8path(get("--alignment"))});
    if (!reportPath.empty()) {
        protectDestination(reportPath,
                           {source, library, fs::u8path(get("--audio")),
                            fs::u8path(get("--alignment"))});
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
    if (options.count("--projection")) {
        session.projection = readProjection(fs::u8path(get("--projection")));
        if (s.algorithm != "umap" && s.algorithm.rfind("umap:", 0) != 0) {
            throw std::runtime_error("Projection requires --algorithm umap");
        }
        s.algorithm = "umap:" + session.projection->identity;
        protectDestination(output, {fs::u8path(get("--projection"))});
        protectDestination(project, {fs::u8path(get("--projection"))});
        if (!reportPath.empty()) {
            protectDestination(reportPath, {fs::u8path(get("--projection"))});
        }
    }
    if (options.count("--audio")) {
        if (!fs::is_regular_file(fs::u8path(get("--audio")))) {
            throw std::runtime_error("Missing audio file");
        }
        session.state.audio =
            fs::absolute(fs::u8path(get("--audio"))).u8string();
    }
    protectDestination(project, {fs::u8path(session.state.audio),
                                 fs::u8path(session.state.reference)});
    if (!reportPath.empty()) {
        protectDestination(reportPath, {fs::u8path(session.state.audio),
                                        fs::u8path(session.state.reference)});
    }
    if (options.count("--alignment-candidate") &&
        !options.count("--alignment") && !options.count("--align")) {
        throw std::runtime_error("Candidate selection requires an alignment");
    }
    if (options.count("--align") && get("--align") == "none") {
        if (options.count("--alignment") ||
            options.count("--alignment-candidate")) {
            throw std::runtime_error(
                "--align none conflicts with alignment artifact/candidate");
        }
        session.setAlignment(1, number("--offset-ms") / 1000, "");
    } else if (options.count("--alignment") || options.count("--align")) {
        if (options.count("--offset-ms")) {
            throw std::runtime_error(
                "Use alignment offset or --offset-ms, not both");
        }
        AlignmentResult result;
        if (options.count("--alignment")) {
            if (options.count("--align")) {
                throw std::runtime_error(
                    "Choose analysis or a saved alignment artifact");
            }
            result = readAlignment(fs::u8path(get("--alignment")));
            if (session.state.audio.empty()) {
                session.state.audio = result.audioPath;
            }
            if (fileHash(fs::u8path(session.state.audio)) != result.audioHash) {
                throw std::runtime_error(
                    "Alignment belongs to another audio file");
            }
        } else {
            if (session.state.audio.empty()) {
                throw std::runtime_error(
                    "Alignment requires --audio or project audio");
            }
            gpp::audio::DeviceSettings device;
            device.device = 0;
            gpp::audio::BassSystem audio(device);
            result =
                analyzeAlignment(session.state, fs::u8path(session.state.audio),
                                 alignmentSettings());
        }
        protectDestination(project, {fs::u8path(session.state.audio)});
        if (!reportPath.empty()) {
            protectDestination(reportPath, {fs::u8path(session.state.audio)});
        }
        if (!result.reliable && !options.count("--alignment-candidate")) {
            const auto json = alignmentJson(result);
            if (!reportPath.empty()) {
                safeWrite(reportPath, json, options.count("--replace"));
            }
            std::cout << json << '\n';
            std::cerr << "Alignment needs review; no chart/project written. "
                         "Use align to save candidates, then --alignment FILE "
                         "--alignment-candidate N to accept.\n";
            return 4;
        }
        const auto candidate = integer("--alignment-candidate", 0);
        if (candidate < 0) {
            throw std::runtime_error("Invalid alignment candidate");
        }
        applyAlignment(session, result, size_t(candidate));
        if (alignmentSilence(result, size_t(candidate)) > 0) {
            std::cerr << "Warning: suggested initial audio silence: "
                      << alignmentSilence(result, size_t(candidate))
                      << " seconds. Audio is unchanged.\n";
        }
    }
    protectDestination(project, {fs::u8path(session.state.audio),
                                 fs::u8path(session.state.reference)});
    if (!reportPath.empty()) {
        protectDestination(reportPath, {fs::u8path(session.state.audio),
                                        fs::u8path(session.state.reference)});
    }
    session.run(loadLibrary(fs::u8path(get("--cache"))), region);
    if (get("--plus") == "none") {
        auto settings = session.state.plus.settings;
        settings.enabled = false;
        session.configurePlus(settings);
    }
    if (get("--plus") == "auto") {
        session.applyPlus(proposePlus(
            session.state, plusSettings(session.state.plus.settings), region));
    }
    exportValidated(session.state, output, options.count("--replace"));
    session.save(project);
    const auto summary =
        get("--plus") == "auto"
            ? plusReport(session.state, plusTrials, uint64_t(plusSeed))
            : report(session.state);
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
