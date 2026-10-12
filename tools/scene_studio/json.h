#pragma once
#include "studio.h"
#include <cereal/external/rapidjson/document.h>
#include <cereal/external/rapidjson/stringbuffer.h>
#include <cereal/external/rapidjson/writer.h>

namespace Studio {
using Json = rapidjson::Document;
using Value = rapidjson::Value;
using Alloc = Json::AllocatorType;

inline Value str(const std::string &s, Alloc &a) {
    return Value(s.c_str(), rapidjson::SizeType(s.size()), a);
}

inline void put(Value &v, const char *key, const std::string &s, Alloc &a) {
    v.AddMember(str(key, a), str(s, a), a);
}

inline void put(Value &v, const char *key, const char *s, Alloc &a) {
    put(v, key, std::string(s), a);
}

inline void put(Value &v, const char *key, bool n, Alloc &a) {
    v.AddMember(str(key, a), Value(n), a);
}

inline void put(Value &v, const char *key, int n, Alloc &a) {
    v.AddMember(str(key, a), Value(n), a);
}

inline void put(Value &v, const char *key, double n, Alloc &a) {
    v.AddMember(str(key, a), Value(n), a);
}

inline void put(Value &v, const char *key, size_t n, Alloc &a) {
    v.AddMember(str(key, a), Value(uint64_t(n)), a);
}

inline void put(Value &v, const char *key, const Vec &vec, Alloc &a) {
    Value arr(rapidjson::kArrayType);
    for (float x : vec) {
        arr.PushBack(x, a);
    }
    v.AddMember(str(key, a), arr, a);
}

inline std::string encode(const Value &value) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    value.Accept(writer);
    return {buffer.GetString(), buffer.GetSize()};
}

inline Json parse(const std::string &s) {
    Json d;
    d.Parse<rapidjson::kParseValidateEncodingFlag>(s.c_str(), s.size());
    if (d.HasParseError()) {
        throw Error("json", "Malformed JSON at byte " +
                                std::to_string(d.GetErrorOffset()));
    }
    return d;
}

inline std::string stringField(const Value &v, const char *key,
                               const std::string &fallback = {}) {
    if (!v.HasMember(key)) {
        return fallback;
    }
    if (!v[key].IsString()) {
        throw Error("parameters", std::string(key) + " must be a string");
    }
    std::string result(v[key].GetString(), v[key].GetStringLength());
    if (result.find('\0') != std::string::npos) {
        throw Error("parameters", "Embedded NUL in string");
    }
    return result;
}

inline double numberField(const Value &v, const char *key, double fallback) {
    if (!v.HasMember(key)) {
        return fallback;
    }
    if (!v[key].IsNumber()) {
        throw Error("parameters", std::string(key) + " must be numeric");
    }
    return v[key].GetDouble();
}

inline int intField(const Value &v, const char *key, int fallback) {
    if (!v.HasMember(key)) {
        return fallback;
    }
    if (!v[key].IsInt()) {
        throw Error("parameters", std::string(key) + " must be an integer");
    }
    return v[key].GetInt();
}

inline Vec vecField(const Value &v, const char *key, Vec fallback) {
    if (!v.HasMember(key)) {
        return fallback;
    }
    const auto &x = v[key];
    if (!x.IsArray() || x.Size() != 3) {
        throw Error("parameters",
                    std::string(key) + " must have three numbers");
    }
    for (rapidjson::SizeType i = 0; i < 3; ++i) {
        if (!x[i].IsNumber()) {
            throw Error("parameters", "Vector components must be numeric");
        }
        fallback[i] = float(x[i].GetDouble());
    }
    return fallback;
}

inline bool boolField(const Value &v, const char *key, bool fallback) {
    if (!v.HasMember(key)) {
        return fallback;
    }
    if (!v[key].IsBool()) {
        throw Error("parameters", std::string(key) + " must be boolean");
    }
    return v[key].GetBool();
}

inline int choice(const std::string &value,
                  std::initializer_list<const char *> choices) {
    int index = 0;
    for (const auto *name : choices) {
        if (value == name) {
            return index;
        }
        ++index;
    }
    throw Error("parameters", "Unknown option: " + value);
}

inline Settings settingsFromJson(const Value &p, Settings s) {
    if (!p.IsObject()) {
        throw Error("parameters", "Settings must be an object");
    }
    for (auto it = p.MemberBegin(); it != p.MemberEnd(); ++it) {
        const std::string key = it->name.GetString();
        if (key != "eye" && key != "target" && key != "up" && key != "fov" &&
            key != "light" && key != "quality" && key != "ao" &&
            key != "shadow_bias" && key != "time" && key != "projection" &&
            key != "navigation" && key != "ortho_height" && key != "speed" &&
            key != "view_mode" && key != "grid" && key != "axes" &&
            key != "bounds") {
            throw Error("parameters", "Unknown setting: " + key);
        }
    }
    if (p.HasMember("navigation")) {
        s.camera.free =
            choice(stringField(p, "navigation"), {"orbit", "free"}) == 1;
        if (s.camera.free) {
            CameraMath::projectionMode(s.camera, false);
        }
    }
    if (p.HasMember("projection")) {
        const bool ortho = choice(stringField(p, "projection"),
                                  {"perspective", "orthographic"}) == 1;
        if (ortho && p.HasMember("navigation") && s.camera.free) {
            throw Error("parameters", "Free navigation requires perspective");
        }
        CameraMath::projectionMode(s.camera, ortho);
    }
    s.camera.eye = vecField(p, "eye", s.camera.eye);
    s.camera.target = vecField(p, "target", s.camera.target);
    s.camera.up = vecField(p, "up", s.camera.up);
    s.camera.fov = float(numberField(p, "fov", s.camera.fov));
    s.camera.orthoHeight =
        float(numberField(p, "ortho_height", s.camera.orthoHeight));
    s.camera.speed = float(numberField(p, "speed", s.camera.speed));
    s.light = vecField(p, "light", s.light);
    s.quality = intField(p, "quality", s.quality);
    s.bias = numberField(p, "shadow_bias", s.bias);
    s.time = numberField(p, "time", s.time);
    s.ao = boolField(p, "ao", s.ao);
    s.grid = boolField(p, "grid", s.grid);
    s.axes = boolField(p, "axes", s.axes);
    s.bounds = boolField(p, "bounds", s.bounds);
    if (p.HasMember("view_mode")) {
        s.viewMode = choice(stringField(p, "view_mode"),
                            {"game", "unlit", "wireframe", "game_wire"});
    }
    return s;
}
} // namespace Studio
