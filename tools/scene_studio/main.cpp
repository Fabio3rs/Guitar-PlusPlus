#include "CLog.h"
#include "json.h"
#include "studio.h"
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <optional>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

namespace {
using namespace Studio;
volatile std::sig_atomic_t stopped = 0;

void stop(int) {
    stopped = 1;
}

struct Options {
    std::string mode = "gui", backend = "glfw", kind = "auto", selection,
                capture, metadata, uiCapture;
    fs::path assets, output = "build/scene-studio";
    int width = 960, height = 540, frames = 0, benchmark = 0, warmup = 10;
    bool json = false, hardware = false, rpc = false;
    Json changes{rapidjson::kObjectType};
};

void help() {
    std::cout
        << "Guitar++ Scene Studio\n"
           "  --mode gui|headless --backend glfw|egl --require-hardware\n"
           "  --scene ID | --scene-file FILE | --model OBJ [--asset-root DIR]\n"
           "  --eye X Y Z --target X Y Z --up X Y Z --fov DEGREES\n"
           "  --light X Y Z --quality 0|1|2|3 --ao on|off --shadow-bias N "
           "--time N\n"
           "  --width W --height H --capture NAME.png --metadata NAME.json\n"
           "  --output-dir DIR (default build/scene-studio) --json\n"
           "  --benchmark FRAMES --warmup FRAMES --frames GUI_FRAMES\n"
           "  --ui-capture NAME.png (GUI only) --rpc-stdio\n"
           "Artifact names are relative to output-dir; absolute paths must "
           "stay inside it.\n";
}

Options options(int argc, char **argv) {
    Options o;
    auto number = [](const std::string &s) {
        size_t end = 0;
        double n = std::stod(s, &end);
        if (end != s.size() || !std::isfinite(n)) {
            throw Error("arguments", "Expected finite number: " + s);
        }
        return n;
    };
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() {
            if (++i >= argc) {
                throw Error("arguments", "Missing value for " + arg);
            }
            return std::string(argv[i]);
        };
        auto integer = [&]() {
            const double v = number(next());
            if (v < 0 || v > 100000 || std::floor(v) != v) {
                throw Error("arguments", "Invalid integer for " + arg);
            }
            return int(v);
        };
        if (arg == "--help") {
            help();
            std::exit(0);
        } else if (arg == "--mode") {
            o.mode = next();
        } else if (arg == "--backend") {
            o.backend = next();
        } else if (arg == "--scene" || arg == "--scene-file" ||
                   arg == "--model") {
            if (o.kind != "auto") {
                throw Error("arguments", "Choose only one scene source");
            }
            o.kind = arg == "--scene"        ? "scene"
                     : arg == "--scene-file" ? "file"
                                             : "model";
            o.selection = next();
        } else if (arg == "--asset-root") {
            o.assets = next();
        } else if (arg == "--output-dir") {
            o.output = next();
        } else if (arg == "--capture") {
            o.capture = next();
        } else if (arg == "--metadata") {
            o.metadata = next();
        } else if (arg == "--ui-capture") {
            o.uiCapture = next();
        } else if (arg == "--width") {
            o.width = integer();
        } else if (arg == "--height") {
            o.height = integer();
        } else if (arg == "--frames") {
            o.frames = integer();
        } else if (arg == "--benchmark") {
            o.benchmark = integer();
        } else if (arg == "--warmup") {
            o.warmup = integer();
        } else if (arg == "--json") {
            o.json = true;
        } else if (arg == "--require-hardware") {
            o.hardware = true;
        } else if (arg == "--rpc-stdio") {
            o.rpc = true;
        } else {
            std::string key = arg.substr(0, 2) == "--" ? arg.substr(2) : arg;
            if (key == "shadow-bias") {
                key = "shadow_bias";
            }
            if (o.changes.HasMember(key.c_str())) {
                throw Error("arguments", "Duplicate parameter: " + arg);
            }
            auto &a = o.changes.GetAllocator();
            if (key == "eye" || key == "target" || key == "up" ||
                key == "light") {
                Vec v{};
                for (auto &n : v) {
                    n = float(number(next()));
                }
                put(o.changes, key.c_str(), v, a);
            } else if (key == "fov" || key == "shadow_bias" || key == "time") {
                put(o.changes, key.c_str(), number(next()), a);
            } else if (key == "quality") {
                put(o.changes, "quality", integer(), a);
            } else if (key == "ao") {
                const auto v = next();
                if (v != "on" && v != "off") {
                    throw Error("arguments", "AO must be on or off");
                }
                put(o.changes, "ao", v == "on", a);
            } else {
                throw Error("arguments", "Unknown option: " + arg);
            }
        }
    }
    if (o.mode != "gui" && o.mode != "headless") {
        throw Error("arguments", "Mode must be gui or headless");
    }
    if (!o.uiCapture.empty() && (o.mode != "gui" || !o.frames)) {
        throw Error("arguments",
                    "UI capture requires GUI and a positive --frames count");
    }
    if (!o.metadata.empty() && o.capture.empty()) {
        throw Error("arguments", "--metadata requires --capture");
    }
    return o;
}

std::string rpc(Session &s, const std::string &line, int width, int height) {
    Json reply(rapidjson::kObjectType);
    auto &a = reply.GetAllocator();
    put(reply, "jsonrpc", "2.0", a);
    Value id;
    int code = -32600;
    try {
        Json req = parse(line);
        if (!req.IsObject() || !req.HasMember("id") ||
            !(req["id"].IsString() || req["id"].IsInt64()) ||
            stringField(req, "jsonrpc") != "2.0") {
            throw Error("request",
                        "Expected JSON-RPC 2.0 request with string/integer id");
        }
        id.CopyFrom(req["id"], a);
        const auto method = stringField(req, "method");
        Value empty(rapidjson::kObjectType);
        const auto &p = req.HasMember("params") ? req["params"] : empty;
        if (!p.IsObject()) {
            throw Error("parameters", "params must be an object");
        }
        code = -32602;
        std::string result;
        const auto start = std::chrono::steady_clock::now();
        if (method == "state" || method == "capabilities" ||
            method == "inspect") {
            result = s.state();
        } else if (method == "load") {
            s.load(stringField(p, "kind", "scene"), stringField(p, "source"));
            result = s.state();
        } else if (method == "set") {
            s.apply(settingsFromJson(p, s.settings));
            result = s.state();
        } else if (method == "reset") {
            s.reset();
            result = s.state();
        } else if (method == "frame") {
            s.frame();
            result = s.state();
        } else if (method == "reload") {
            s.reload();
            result = s.state();
        } else if (method == "render") {
            s.render(s.viewport, intField(p, "width", width),
                     intField(p, "height", height));
            result = s.state();
        } else if (method == "capture") {
            result = s.capture(
                stringField(p, "path"), intField(p, "width", width),
                intField(p, "height", height), stringField(p, "metadata"));
        } else if (method == "benchmark") {
            result = s.benchmark(
                intField(p, "frames", 100), intField(p, "warmup", 10),
                intField(p, "width", width), intField(p, "height", height),
                numberField(p, "timeout", 30));
        } else if (method == "shutdown") {
            stopped = 1;
            result = "{\"stopping\":true}";
        } else {
            code = -32601;
            throw Error("method", "Unknown method: " + method);
        }
        if (method != "benchmark" &&
            std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                          start)
                    .count() > 30) {
            throw Error(
                "timeout",
                "Operation exceeded 30 seconds; inspect state before retrying");
        }
        auto value = parse(result);
        Value copied;
        copied.CopyFrom(value, a);
        reply.AddMember("result", copied, a);
    } catch (const std::exception &e) {
        const auto *detail = dynamic_cast<const Error *>(&e);
        if (detail && detail->code == "json") {
            code = -32700;
        } else if (detail && detail->code != "request" &&
                   detail->code != "parameters" && detail->code != "method") {
            code = -32000;
        }
        Value error(rapidjson::kObjectType);
        put(error, "code", code, a);
        put(error, "message", e.what(), a);
        put(error, "data", detail ? detail->code : "internal", a);
        reply.AddMember("error", error, a);
    }
    reply.AddMember("id", id, a);
    return encode(reply);
}

// Readiness polling keeps all GL operations on the main thread. Bounded input,
// at most one request per frame; no network listener or subprocess registry.
class Input {
    std::string buffer;
    bool dropping = false;

  public:
    bool eof = false;

    std::optional<std::string> read(bool block) {
        if (auto pos = buffer.find('\n'); pos != std::string::npos) {
            auto line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            return line;
        }
        char bytes[4096];
        int count = 0;
#ifdef _WIN32
        const HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
        DWORD available = 0;
        if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr)) {
            eof = true;
            return {};
        }
        if (!available) {
            if (block) {
                Sleep(10);
            }
            return {};
        }
        count = _read(_fileno(stdin), bytes,
                      int(std::min<DWORD>(available, sizeof(bytes))));
#else
        pollfd descriptor{STDIN_FILENO, POLLIN, 0};
        if (poll(&descriptor, 1, block ? 50 : 0) <= 0) {
            return {};
        }
        count = int(::read(STDIN_FILENO, bytes, sizeof(bytes)));
#endif
        if (count <= 0) {
            eof = true;
            return {};
        }
        for (int i = 0; i < count; ++i) {
            if (dropping) {
                if (bytes[i] == '\n') {
                    dropping = false;
                }
                continue;
            }
            buffer += bytes[i];
            if (buffer.size() > 65536) {
                buffer.clear();
                dropping = true;
                std::cout
                    << "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-"
                       "32600,\"message\":\"Request exceeds 64 KiB\"}}\n"
                    << std::flush;
            }
        }
        return {};
    }
};
} // namespace

int main(int argc, char **argv) {
    using namespace Studio;
    CLog::configureStderr();
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    bool json = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--json") {
            json = true;
        }
    }
    try {
        auto o = options(argc, argv);
        Context context(o.backend, o.mode == "gui", o.hardware);
        Session session(context, o.assets, o.output);
        session.load(o.kind, o.selection);
        session.apply(settingsFromJson(o.changes, session.settings));
        session.render(session.viewport, o.width, o.height);
        std::string result = session.state();
        if (o.benchmark) {
            result =
                session.benchmark(o.benchmark, o.warmup, o.width, o.height);
        }
        if (!o.capture.empty()) {
            result = session.capture(o.capture, o.width, o.height, o.metadata);
        }
        std::unique_ptr<UI> ui;
        if (o.mode == "gui") {
            ui = std::make_unique<UI>(session);
        }
        if (o.rpc) {
            std::cerr << "Scene Studio RPC ready (stdio)\n";
        }
        Input input;
        for (int frame = 0; !stopped && (ui || o.rpc); ++frame) {
            if (context.window) {
                glfwPollEvents();
                if (glfwWindowShouldClose(context.window)) {
                    break;
                }
            }
            if (o.rpc) {
                auto line = input.read(!ui);
                if (line) {
                    std::cout << rpc(session, *line, o.width, o.height) << '\n'
                              << std::flush;
                }
                if (input.eof) {
                    break;
                }
            }
            if (ui) {
                ui->draw(session);
                if (o.frames && frame + 1 >= o.frames && !o.uiCapture.empty()) {
                    int w = 0, h = 0;
                    glfwGetFramebufferSize(context.window, &w, &h);
                    glReadBuffer(GL_BACK);
                    const auto path = session.artifact(o.uiCapture);
                    if (path.extension() != ".png") {
                        throw Error("path",
                                    "UI capture filename must end in .png");
                    }
                    writePNG(path, w, h, readPixels(w, h));
                    Json r = parse(session.state());
                    auto &a = r.GetAllocator();
                    put(r, "capture", path.string(), a);
                    put(r, "image_kind", "full_ui", a);
                    put(r, "width", w, a);
                    put(r, "height", h, a);
                    result = encode(r);
                    std::ofstream metadata(path.string() + ".json");
                    metadata << result << '\n';
                    metadata.close();
                    if (!metadata) {
                        throw Error("capture",
                                    "Cannot write UI capture metadata");
                    }
                }
                glfwSwapBuffers(context.window);
                if (o.frames && frame + 1 >= o.frames) {
                    break;
                }
            }
        }
        if (!o.rpc) {
            if (o.json) {
                std::cout << result << '\n';
            } else {
                std::cout << "Scene Studio completed: " << session.sceneLabel()
                          << '\n';
            }
        }
        return 0;
    } catch (const std::exception &e) {
        const auto *detail = dynamic_cast<const Error *>(&e);
        std::cerr << "Scene Studio: " << e.what() << '\n';
        if (json) {
            Json result(rapidjson::kObjectType);
            auto &a = result.GetAllocator();
            put(result, "ok", false, a);
            put(result, "code", detail ? detail->code : "internal", a);
            put(result, "message", e.what(), a);
            std::cout << encode(result) << '\n';
        }
        return 1;
    }
}
