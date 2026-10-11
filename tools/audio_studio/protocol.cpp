#include "session.h"
#include <cereal/external/rapidjson/stringbuffer.h>
#include <cereal/external/rapidjson/writer.h>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <type_traits>
#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

namespace AudioStudio {
namespace {
using Alloc = Json::AllocatorType;
Value str(const std::string &s, Alloc &a) {
    return Value(s.c_str(), rapidjson::SizeType(s.size()), a);
}
void put(Value &v, const char *key, const std::string &s, Alloc &a) {
    v.AddMember(str(key, a), str(s, a), a);
}
void put(Value &v, const char *key, const char *s, Alloc &a) {
    put(v, key, std::string(s), a);
}
void put(Value &v, const char *key, bool x, Alloc &a) {
    v.AddMember(str(key, a), Value(x), a);
}
void put(Value &v, const char *key, double x, Alloc &a) {
    v.AddMember(str(key, a), Value(x), a);
}
void put(Value &v, const char *key, int x, Alloc &a) {
    v.AddMember(str(key, a), Value(x), a);
}
void put(Value &v, const char *key, uint64_t x, Alloc &a) {
    v.AddMember(str(key, a), Value(x), a);
}
std::string stringField(const Value &v, const char *key,
                        const std::string &fallback = {}) {
    if (!v.HasMember(key)) {
        return fallback;
    }
    if (!v[key].IsString()) {
        throw audio::Error(std::string(key) + " must be a string");
    }
    std::string s(v[key].GetString(), v[key].GetStringLength());
    if (s.find('\0') != std::string::npos) {
        throw audio::Error("NUL in string");
    }
    return s;
}

double number(const Value &v, const char *key, double fallback) {
    if (!v.HasMember(key)) {
        return fallback;
    }
    if (!v[key].IsNumber() || !std::isfinite(v[key].GetDouble())) {
        throw audio::Error(std::string(key) + " must be finite");
    }
    return v[key].GetDouble();
}

int integer(const Value &v, const char *key, int fallback) {
    const double x = number(v, key, fallback);
    if (std::floor(x) != x || x < -2147483648. || x > 2147483647.) {
        throw audio::Error("Expected integer");
    }
    return int(x);
}

bool boolean(const Value &v, const char *key, bool fallback) {
    if (!v.HasMember(key)) {
        return fallback;
    }
    if (!v[key].IsBool()) {
        throw audio::Error(std::string(key) + " must be boolean");
    }
    return v[key].GetBool();
}

void addLevels(Value &out, const char *key, const audio::Levels &levels,
               Alloc &a) {
    Value v(rapidjson::kObjectType);
    put(v, "peak", levels.peak, a);
    put(v, "rms", levels.rms, a);
    put(v, "clipped_samples", uint64_t(levels.clipped), a);
    out.AddMember(str(key, a), v, a);
}
} // namespace

std::string encode(const Value &value) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    if (!value.Accept(writer)) {
        throw audio::Error("Cannot encode JSON result");
    }
    return buffer.GetString();
}

Json parseJson(const std::string &text) {
    if (text.size() > 1024 * 1024) {
        throw audio::Error("JSON exceeds 1 MiB limit");
    }
    Json json;
    json.Parse<rapidjson::kParseValidateEncodingFlag>(text.c_str(),
                                                      text.size());
    if (json.HasParseError()) {
        throw audio::Error("Malformed JSON");
    }
    return json;
}

Json settingsJson(const audio::Settings &settings) {
    Json json(rapidjson::kObjectType);
    auto &a = json.GetAllocator();
    auto copy = settings;
    audio::visitSettings(copy,
                         [&](const std::string &name, auto &value, double,
                             double) { put(json, name.c_str(), value, a); });
    return json;
}

audio::Settings parseSettings(const Value &json, audio::Settings base) {
    if (!json.IsObject()) {
        throw audio::Error("Settings must be an object");
    }
    for (auto member = json.MemberBegin(); member != json.MemberEnd();
         ++member) {
        const std::string key(member->name.GetString(),
                              member->name.GetStringLength());
        bool found = false;
        audio::visitSettings(base, [&](const std::string &name, auto &field,
                                       double low, double high) {
            if (name != key) {
                return;
            }
            found = true;
            using T = std::decay_t<decltype(field)>;
            if constexpr (std::is_same_v<T, bool>) {
                if (!member->value.IsBool()) {
                    throw audio::Error(name + " must be boolean");
                }
                field = member->value.GetBool();
            } else {
                if (!member->value.IsNumber()) {
                    throw audio::Error(name + " must be numeric");
                }
                const double n = member->value.GetDouble();
                if (!std::isfinite(n) || n < low || n > high) {
                    throw audio::Error(name + " outside supported range");
                }
                if constexpr (std::is_integral_v<T>) {
                    if (std::floor(n) != n) {
                        throw audio::Error(name + " must be integral");
                    }
                }
                field = T(n);
            }
        });
        if (!found) {
            throw audio::Error("Unknown setting: " + key);
        }
    }
    audio::validate(base);
    return base;
}

void savePreset(const fs::path &file, const audio::Settings &settings) {
    Json json(rapidjson::kObjectType);
    auto &a = json.GetAllocator();
    put(json, "version", 1, a);
    auto values = settingsJson(settings);
    json.AddMember("settings", Value(values, a), a);
    // Atomic directory creation reserves a private sibling namespace, including
    // across Studio processes. Never truncate an existing .tmp file.
    fs::path scratch;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        auto candidate = file;
        candidate += ".tmp-" + std::to_string(stamp) + "-" + std::to_string(attempt);
        if (fs::create_directory(candidate)) {
            scratch = std::move(candidate);
            break;
        }
    }
    if (scratch.empty()) {
        throw audio::Error("Cannot reserve preset temporary directory");
    }
    const auto temporary = scratch / "preset";
    try {
        std::ofstream out(temporary);
        out << encode(json) << '\n';
        out.close();
        if (!out) {
            throw audio::Error("Cannot write preset");
        }
        fs::rename(temporary, file);
    } catch (...) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        fs::remove(scratch, ignored);
        throw;
    }
    std::error_code ignored;
    fs::remove(scratch, ignored);
}

audio::Settings loadPreset(const fs::path &file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        throw audio::Error("Cannot read preset: " + file.string());
    }
    std::string text(1024 * 1024 + 1, '\0');
    in.read(text.data(), std::streamsize(text.size()));
    text.resize(size_t(in.gcount()));
    auto json = parseJson(text);
    if (!json.IsObject() || !json.HasMember("version") ||
        !json["version"].IsInt() || json["version"].GetInt() != 1 ||
        !json.HasMember("settings")) {
        throw audio::Error("Expected version 1 Studio preset");
    }
    return parseSettings(json["settings"]);
}

Json stateJson(Session &s) {
    Json json(rapidjson::kObjectType);
    auto &a = json.GetAllocator();
    put(json, "source", s.source.string(), a);
    put(json, "busy", s.busy(), a);
    put(json, "job_id", s.revision(), a);
    put(json, "job", s.jobKind, a);
    put(json, "progress", s.progress(), a);
    put(json, "error", s.error, a);
    put(json, "notice", s.notice, a);
    put(json, "cursor", s.cursor, a);
    put(json, "selection_start", s.selectionStart, a);
    put(json, "selection_end", s.selectionEnd, a);
    put(json, "position", s.music.position(), a);
    put(json, "preview_mode", s.previewMode, a);
    put(json, "playback_state", int(BASS_ChannelIsActive(s.music.handle())), a);
    put(json, "preview_state", int(BASS_ChannelIsActive(s.preview.handle())),
        a);
    put(json, "music_loop_error", s.music.loopError(), a);
    put(json, "preview_loop_error", s.preview.loopError(), a);
    put(json, "device", int(BASS_GetDevice()), a);
    put(json, "audible_output", BASS_GetDevice() != 0, a);
    put(json, "bass_version", int(BASS_GetVersion()), a);
    put(json, "fx_version", int(BASS_FX_GetVersion()), a);
    put(json, "bass_cpu_percent", double(BASS_GetCPU()), a);
    put(json, "seek_api_ms", s.music.lastSeekMs, a);
    put(json, "play_api_ms", s.preview.lastPlayMs, a);
    put(json, "preview_prepare_ms", s.previewPrepareMs, a);
    json.AddMember("audible_latency_ms", Value(), a);
    BASS_INFO deviceInfo{};
    if (BASS_GetInfo(&deviceInfo)) {
        put(json, "device_rate", int(deviceInfo.freq), a);
        put(json, "device_minbuf_ms", int(deviceInfo.minbuf), a);
    }
    Value config(rapidjson::kObjectType);
    put(config, "channel_buffer_ms", int(BASS_GetConfig(BASS_CONFIG_BUFFER)),
        a);
    put(config, "channel_update_ms",
        int(BASS_GetConfig(BASS_CONFIG_UPDATEPERIOD)), a);
    put(config, "device_buffer_ms", int(BASS_GetConfig(BASS_CONFIG_DEV_BUFFER)),
        a);
    put(config, "device_period_ms", int(BASS_GetConfig(BASS_CONFIG_DEV_PERIOD)),
        a);
    json.AddMember("buffer_config", config, a);
    auto settings = settingsJson(s.settings);
    json.AddMember("settings", Value(settings, a), a);
    auto musicSettings = s.settings;
    musicSettings.fx = s.musicFx;
    auto musicJson = settingsJson(musicSettings);
    json.AddMember("music_settings", Value(musicJson, a), a);
    if (s.wave) {
        put(json, "duration", s.wave->info.duration, a);
        put(json, "rate", int(s.wave->info.rate), a);
        put(json, "channels", int(s.wave->info.channels), a);
        put(json, "waveform_bins", uint64_t(s.wave->low.size()), a);
    }
    if (s.product) {
        const auto &p = *s.product;
        put(json, "analysis_ms", p.analysis.milliseconds, a);
        put(json, "synth_ms", p.synthMs, a);
        put(json, "fx_ms", p.fxMs, a);
        put(json, "result_time", p.time, a);
        put(json, "pcm_bytes",
            uint64_t((p.raw.samples.size() + p.processed.samples.size()) *
                     sizeof(float)),
            a);
        put(json, "raw_seconds", p.raw.seconds(), a);
        put(json, "processed_seconds", p.processed.seconds(), a);
        put(json, "loop_begin", uint64_t(p.processed.loop.begin), a);
        put(json, "loop_end", uint64_t(p.processed.loop.end), a);
        addLevels(json, "raw_levels", p.rawLevels, a);
        addLevels(json, "processed_levels", p.processedLevels, a);
        Value notes(rapidjson::kArrayType), spectrum(rapidjson::kArrayType);
        for (const auto &note : p.analysis.notes) {
            Value v(rapidjson::kObjectType);
            put(v, "midi", note.midi, a);
            put(v, "name", note.name, a);
            put(v, "hz", note.frequency, a);
            put(v, "score", note.score, a);
            notes.PushBack(v, a);
        }
        for (float value : p.analysis.spectrum) {
            spectrum.PushBack(double(value), a);
        }
        json.AddMember("notes", notes, a);
        json.AddMember("spectrum", spectrum, a);
    }
    return json;
}

std::string dispatch(Session &s, const std::string &line) {
    Json response(rapidjson::kObjectType);
    auto &a = response.GetAllocator();
    put(response, "jsonrpc", "2.0", a);
    Value id;
    try {
        auto request = parseJson(line);
        if (!request.IsObject() || stringField(request, "jsonrpc") != "2.0" ||
            !request.HasMember("id") ||
            !(request["id"].IsString() || request["id"].IsInt64())) {
            throw audio::Error("Expected JSON-RPC 2.0 request with id");
        }
        id.CopyFrom(request["id"], a);
        const std::string method = stringField(request, "method");
        const Value empty(rapidjson::kObjectType);
        const auto &p = request.HasMember("params") ? request["params"] : empty;
        if (!p.IsObject()) {
            throw audio::Error("params must be an object");
        }
        Value result(rapidjson::kObjectType);
        uint64_t job = 0;
        if (method == "state") {
            auto state = stateJson(s);
            result.CopyFrom(state, a);
        } else if (method == "load") {
            job = s.load(stringField(p, "path"));
        } else if (method == "set") {
            s.updateSettings(parseSettings(p, s.settings));
        } else if (method == "music.fx") {
            auto next = s.settings;
            next.fx = s.musicFx;
            s.applyMusicFx(parseSettings(p, next).fx);
        } else if (method == "seek") {
            s.seek(number(p, "time", s.cursor));
        } else if (method == "selection") {
            const double start = number(p, "start", s.selectionStart),
                         end = number(p, "end", s.selectionEnd);
            s.setSelection(start, end);
        } else if (method == "analyze" || method == "generate") {
            job = s.generate(method == "generate");
        } else if (method == "cancel") {
            s.cancel();
        } else if (method == "export") {
            job = s.exportAudio(
                stringField(p, "path"), boolean(p, "segment", false),
                boolean(p, "raw", false), boolean(p, "float32", false));
        } else if (method == "preset.save") {
            savePreset(s.artifact(stringField(p, "path")), s.settings);
        } else if (method == "preset.load") {
            s.updateSettings(loadPreset(stringField(p, "path")));
        } else if (method == "preview") {
            s.playPreview(stringField(p, "mode", "processed"));
        } else if (method == "transport") {
            const auto target = stringField(p, "target", "music");
            if (target != "music" && target != "preview") {
                throw audio::Error("Unknown transport target");
            }
            auto &channel = target == "music" ? s.music : s.preview;
            const auto action = stringField(p, "action");
            if (action == "play") {
                channel.play();
            } else if (action == "pause") {
                channel.pause();
            } else if (action == "stop") {
                channel.stop();
            } else if (action == "controls") {
                const float volume = float(number(
                    p, "volume",
                    target == "music" ? s.musicVolume : s.previewVolume));
                const float pan = float(number(
                    p, "pan", target == "music" ? s.musicPan : s.previewPan));
                const float buffer =
                    float(number(p, "buffer_seconds", s.streamBuffer));
                channel.controls(volume, pan, buffer);
                if (target == "music") {
                    s.musicVolume = volume;
                    s.musicPan = pan;
                } else {
                    s.previewVolume = volume;
                    s.previewPan = pan;
                }
                s.streamBuffer = buffer;
            } else if (action == "loop") {
                const bool enabled = boolean(p, "enabled", true);
                if (target == "music") {
                    s.setMusicLoop(number(p, "start", s.selectionStart),
                                   number(p, "end", s.selectionEnd), enabled);
                } else {
                    channel.loop(number(p, "start", s.selectionStart),
                                 number(p, "end", s.selectionEnd), enabled);
                }
            } else {
                throw audio::Error("Unknown transport action");
            }
        } else if (method == "device.list") {
            result.SetArray();
            for (const auto &device : audio::devices()) {
                Value v(rapidjson::kObjectType);
                put(v, "id", device.id, a);
                put(v, "name", device.name, a);
                put(v, "enabled", device.enabled, a);
                put(v, "default", device.isDefault, a);
                result.PushBack(v, a);
            }
        } else if (method == "device.apply") {
            auto d = s.deviceSettings;
            d.device = integer(p, "device", d.device);
            d.rate = integer(p, "rate", d.rate);
            d.bufferMs = integer(p, "buffer_ms", d.bufferMs);
            d.updateMs = integer(p, "update_ms", d.updateMs);
            d.deviceBufferMs = integer(p, "device_buffer_ms", d.deviceBufferMs);
            d.devicePeriodMs = integer(p, "device_period_ms", d.devicePeriodMs);
            s.applyDevice(d);
        } else if (method == "slot.store") {
            s.storeSlot(integer(p, "slot", 0));
        } else if (method == "slot.recall") {
            s.recallSlot(integer(p, "slot", 0));
        } else if (method == "shutdown") {
            s.shutdown = true;
        } else {
            throw audio::Error("Unknown method: " + method);
        }
        if (job) {
            put(result, "job_id", job, a);
        }
        response.AddMember("result", result, a);
    } catch (const std::exception &e) {
        Value error(rapidjson::kObjectType);
        put(error, "code", -32602, a);
        put(error, "message", e.what(), a);
        response.AddMember("error", error, a);
    }
    response.AddMember("id", id, a);
    return encode(response);
}

void StdinPump::poll(Session &session) {
    if (ended) {
        return;
    }
    char bytes[4096];
    int count = 0;
#ifdef _WIN32
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD available = 0, got = 0;
    if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr)) {
        ended = true;
        return;
    }
    if (!available) {
        return;
    }
    if (!ReadFile(input, bytes, sizeof(bytes), &got, nullptr)) {
        ended = true;
        return;
    }
    count = int(got);
#else
    pollfd fd{STDIN_FILENO, POLLIN, 0};
    if (::poll(&fd, 1, 0) <= 0) {
        return;
    }
    count = int(::read(STDIN_FILENO, bytes, sizeof(bytes)));
#endif
    if (count <= 0) {
        ended = true;
    } else {
        buffered_.append(bytes, size_t(count));
    }
    if (buffered_.size() > 1024 * 1024) {
        std::cerr << "RPC input exceeds 1 MiB limit\n";
        ended = true;
        session.shutdown = true;
        return;
    }
    for (;;) {
        const auto end = buffered_.find('\n');
        if (end == std::string::npos) {
            break;
        }
        const auto line = buffered_.substr(0, end);
        buffered_.erase(0, end + 1);
        if (!line.empty()) {
            std::cout << dispatch(session, line) << std::endl;
        }
        if (session.shutdown) {
            break;
        }
    }
    if (ended && !buffered_.empty()) {
        std::cout << dispatch(session, buffered_) << std::endl;
        buffered_.clear();
    }
}
} // namespace AudioStudio
