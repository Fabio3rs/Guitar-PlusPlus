#include "session.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

namespace {
using namespace AudioStudio;
struct Options {
    std::string mode = "gui", capture, preset, file, exportFile;
    fs::path output = "build/audio-studio";
    bool rpc = false, json = false, analysisOnly = false, segment = false,
         float32 = false;
    int frames = 0, benchmark = 0;
    double time = 0, length = .2;
    audio::DeviceSettings device;
    bool explicitDevice = false;
    std::vector<std::pair<std::string, std::string>> changes;
};

Options parse(int argc, char **argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() {
            if (++i >= argc) {
                throw audio::Error("Missing value for " + arg);
            }
            return std::string(argv[i]);
        };
        auto numeric = [&]() {
            const std::string text = next();
            size_t used = 0;
            const double n = std::stod(text, &used);
            if (used != text.size() || !std::isfinite(n)) {
                throw audio::Error("Expected finite number for " + arg);
            }
            return n;
        };
        auto integer = [&]() {
            const double n = numeric();
            if (n < -1 || n > 1000000 || n != std::floor(n)) {
                throw audio::Error("Expected bounded integer for " + arg);
            }
            return int(n);
        };
        if (arg == "--help") {
            std::cout
                << "Guitar++ Audio Studio\n"
                   "  --mode gui|headless --file AUDIO --time SECONDS --length "
                   "SECONDS\n"
                   "  --preset FILE --set KEY=JSON_VALUE (repeatable)\n"
                   "  --export NAME.wav [--segment] [--float32] "
                   "--analysis-only\n"
                   "  --device ID (0 = No Sound) --benchmark COUNT --json "
                   "--rpc-stdio\n"
                   "  --output-dir DIR --frames COUNT --ui-capture NAME.png\n"
                   "Default output: build/audio-studio. Exports stay inside "
                   "output-dir.\n"
                   "Headless defaults to No Sound and performs no graphical "
                   "initialization.\n";
            std::exit(0);
        } else if (arg == "--mode") {
            o.mode = next();
        } else if (arg == "--file") {
            o.file = next();
        } else if (arg == "--preset") {
            o.preset = next();
        } else if (arg == "--output-dir") {
            o.output = next();
        } else if (arg == "--export") {
            o.exportFile = next();
        } else if (arg == "--time") {
            o.time = numeric();
        } else if (arg == "--length") {
            o.length = numeric();
        } else if (arg == "--frames") {
            o.frames = integer();
        } else if (arg == "--benchmark") {
            o.benchmark = integer();
        } else if (arg == "--device") {
            o.device.device = integer();
            o.explicitDevice = true;
        } else if (arg == "--ui-capture") {
            o.capture = next();
        } else if (arg == "--rpc-stdio") {
            o.rpc = true;
        } else if (arg == "--json") {
            o.json = true;
        } else if (arg == "--analysis-only") {
            o.analysisOnly = true;
        } else if (arg == "--segment") {
            o.segment = true;
        } else if (arg == "--float32") {
            o.float32 = true;
        } else if (arg == "--set") {
            const auto value = next();
            const auto equal = value.find('=');
            if (equal == std::string::npos) {
                throw audio::Error("--set requires KEY=JSON_VALUE");
            }
            o.changes.emplace_back(value.substr(0, equal),
                                   value.substr(equal + 1));
        } else {
            throw audio::Error("Unknown argument: " + arg);
        }
    }
    if (o.mode != "gui" && o.mode != "headless") {
        throw audio::Error("Mode must be gui or headless");
    }
    if (o.time < 0 || o.length <= 0 || o.frames < 0 || o.benchmark < 0) {
        throw audio::Error("Invalid time/count argument");
    }
    if (!o.capture.empty() && (o.mode != "gui" || !o.frames)) {
        throw audio::Error("UI capture requires GUI and --frames");
    }
    if (o.mode == "headless" && !o.explicitDevice) {
        o.device.device = 0;
    }
    if (o.rpc && (o.json || !o.exportFile.empty() || o.benchmark)) {
        throw audio::Error(
            "RPC cannot be combined with one-shot JSON/export/benchmark");
    }
    return o;
}
} // namespace

int main(int argc, char **argv) {
    using namespace AudioStudio;
    try {
        const auto o = parse(argc, argv);
        Session session(o.device, o.output);
        auto settings =
            o.preset.empty() ? audio::Settings{} : loadPreset(o.preset);
        for (const auto &change : o.changes) {
            Json patch(rapidjson::kObjectType);
            auto value = parseJson(change.second);
            Value key(change.first.c_str(), patch.GetAllocator());
            patch.AddMember(key, Value(value, patch.GetAllocator()),
                            patch.GetAllocator());
            settings = parseSettings(patch, settings);
        }
        session.updateSettings(settings);
        if (!o.file.empty()) {
            session.load(o.file);
            session.wait();
            session.seek(o.time);
            session.setSelection(o.time,
                std::min(session.wave->info.duration, o.time + o.length));
        }
        if (!o.rpc && !o.file.empty()) {
            session.generate(!o.analysisOnly);
            session.wait();
        }
        if (o.benchmark) {
            if (session.source.empty()) {
                throw audio::Error("Benchmark requires --file");
            }
            double analysis = 0, synth = 0, fx = 0;
            for (int i = 0; i < o.benchmark; ++i) {
                session.generate(!o.analysisOnly);
                session.wait();
                analysis += session.product->analysis.milliseconds;
                synth += session.product->synthMs;
                fx += session.product->fxMs;
            }
            Json report(rapidjson::kObjectType);
            auto &a = report.GetAllocator();
            report.AddMember("iterations", o.benchmark, a);
            report.AddMember("analysis_mean_ms", analysis / o.benchmark, a);
            report.AddMember("synth_mean_ms", synth / o.benchmark, a);
            report.AddMember("fx_mean_ms", fx / o.benchmark, a);
            report.AddMember("audible_latency_ms", Value(), a);
            std::cout << encode(report) << '\n';
        }
        if (!o.exportFile.empty()) {
            session.exportAudio(o.exportFile, o.segment, false, o.float32);
            session.wait();
        }
        if (o.mode == "gui") {
            return runGui(session, o.rpc, o.frames, o.capture);
        }
        if (o.rpc) {
            StdinPump input;
            while (!session.shutdown) {
                session.tick();
                input.poll(session);
                if (input.ended && !session.busy()) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        } else if (o.json) {
            std::cout << encode(stateJson(session)) << '\n';
        } else if (!o.benchmark) {
            std::cout << "Audio Studio: " << session.source.string() << '\n';
            if (session.product) {
                std::cout << "Generated "
                          << session.product->processed.seconds() << " s; FFT "
                          << session.product->analysis.milliseconds
                          << " ms, synthesis " << session.product->synthMs
                          << " ms, FX " << session.product->fxMs << " ms\n";
            }
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Audio Studio: " << e.what() << '\n';
        return 1;
    }
}
