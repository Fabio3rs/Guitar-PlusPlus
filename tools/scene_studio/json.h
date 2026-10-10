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

inline Settings settingsFromJson(const Value &p, Settings s) {
    if (!p.IsObject()) {
        throw Error("parameters", "Settings must be an object");
    }
    for (auto it = p.MemberBegin(); it != p.MemberEnd(); ++it) {
        const std::string key = it->name.GetString();
        if (key != "eye" && key != "target" && key != "up" && key != "fov" &&
            key != "light" && key != "quality" && key != "ao" &&
            key != "shadow_bias" && key != "time") {
            throw Error("parameters", "Unknown setting: " + key);
        }
    }
    s.camera.eye = vecField(p, "eye", s.camera.eye);
    s.camera.target = vecField(p, "target", s.camera.target);
    s.camera.up = vecField(p, "up", s.camera.up);
    s.camera.fov = float(numberField(p, "fov", s.camera.fov));
    s.light = vecField(p, "light", s.light);
    s.quality = intField(p, "quality", s.quality);
    s.bias = numberField(p, "shadow_bias", s.bias);
    s.time = numberField(p, "time", s.time);
    if (p.HasMember("ao")) {
        if (!p["ao"].IsBool()) {
            throw Error("parameters", "ao must be boolean");
        }
        s.ao = p["ao"].GetBool();
    }
    return s;
}
} // namespace Studio
