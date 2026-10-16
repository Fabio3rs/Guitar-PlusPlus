#include "midi_synth.h"
#include <algorithm>
#include <iostream>
namespace ChartStudio {
int synthCli(int argc, char **argv) {
    if (argc < 3) {
        throw std::runtime_error(
            "render-stems PROJECT --soundfont FILE.sf2 --output-dir DIR "
            "[--instruments chart|all|custom] [--track N] [--stem "
            "NAME:TRACK[:CHANNEL[:PROGRAM]]] [--format wav|ogg] [--sample-rate "
            "44100] [--quality 5] [--gain 0.5] [--tail 2] [--save-config]");
    }
    Session session;
    session.openProject(fs::u8path(argv[2]));
    auto c = session.state.synth;
    bool save = false, selection = false;
    for (int i = 3; i < argc; ++i) {
        std::string flag = argv[i];
        if (flag == "--save-config") {
            save = true;
            continue;
        }
        if (++i == argc) {
            throw std::runtime_error("Missing value for " + flag);
        }
        std::string value = argv[i];
        if (flag == "--soundfont") {
            c.soundfont = fs::absolute(fs::u8path(value)).u8string();
        } else if (flag == "--output-dir") {
            c.output = fs::absolute(fs::u8path(value)).u8string();
        } else if (flag == "--format") {
            c.format = value;
        } else if (flag == "--sample-rate") {
            c.sampleRate = std::stoi(value);
        } else if (flag == "--quality") {
            c.quality = std::stoi(value);
        } else if (flag == "--gain") {
            c.gain = std::stod(value);
        } else if (flag == "--tail") {
            c.tail = std::stod(value);
        } else if (flag == "--instruments") {
            c.mode = value;
            c.stems.clear();
        } else if (flag == "--track" || flag == "--stem") {
            if (!selection) {
                c.stems.clear();
                selection = true;
            }
            c.mode = "custom";
            std::vector<std::string> fields;
            std::istringstream input(value);
            std::string field;
            while (std::getline(input, field, ':')) {
                fields.push_back(field);
            }
            SynthSource source;
            std::string name;
            if (flag == "--track") {
                source.track = std::stoi(value);
                name = "track" + value;
            } else {
                if (fields.size() < 2 || fields.size() > 4) {
                    throw std::runtime_error(
                        "Expected NAME:TRACK[:CHANNEL[:PROGRAM]]");
                }
                name = fields[0];
                source.track = std::stoi(fields[1]);
                if (fields.size() > 2) {
                    source.channel = std::stoi(fields[2]);
                }
                if (fields.size() > 3) {
                    source.program = std::stoi(fields[3]);
                }
            }
            auto it = std::find_if(
                c.stems.begin(), c.stems.end(),
                [&](const SynthStem &stem) { return stem.name == name; });
            if (it == c.stems.end()) {
                c.stems.push_back({name, true, {source}});
            } else {
                it->sources.push_back(source);
            }
        } else {
            throw std::runtime_error("Unknown synthesis option: " + flag);
        }
    }
    renderStems(session.state, c);
    if (save) {
        session.state.synth = c;
        session.save(fs::u8path(argv[2]));
    }
    std::cout << "Stems exported to " << c.output << "\n";
    return 0;
}
} // namespace ChartStudio
