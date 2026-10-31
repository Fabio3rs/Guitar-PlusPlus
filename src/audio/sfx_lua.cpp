#include "Lua/lua.hpp"
#include "sfx.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace gpp::audio::sfx {
namespace {
namespace fs = std::filesystem;
struct Budget {
    size_t used = 0, limit = 8 * 1024 * 1024;
};

void *allocate(void *user, void *ptr, size_t old, size_t size) {
    auto &b = *static_cast<Budget *>(user);
    if (!ptr) {
        old = 0;
    }

    if (!size) {
        b.used -= old;
        std::free(ptr);
        return nullptr;
    }

    if (size > old && size - old > b.limit - b.used) {
        return nullptr;
    }

    void *next = std::realloc(ptr, size);
    if (next) {
        b.used = b.used - old + size;
    }

    return next;
}

struct Table {
    lua_State *L;
    int index;
    std::string path;
    Table(lua_State *state, std::string p)
        : L(state), index(lua_gettop(L)), path(std::move(p)) {
        if (!lua_istable(L, index)) {
            throw Error(path + " must be a table");
        }
    }

    void keys(std::initializer_list<const char *> allowed) const {
        lua_pushnil(L);
        while (lua_next(L, index)) {
            if (lua_type(L, -2) != LUA_TSTRING) {
                throw Error(path + " has a non-string field");
            }

            size_t length = 0;
            const char *key = lua_tolstring(L, -2, &length);
            const std::string value(key, length);
            if (std::find(allowed.begin(), allowed.end(), value) ==
                allowed.end()) {
                throw Error(path + "." + value + " is not a supported field");
            }

            lua_pop(L, 1);
        }
    }

    bool push(const char *key) const {
        lua_pushstring(L, key);
        lua_rawget(L, index);
        return !lua_isnil(L, -1);
    }

    double number(const char *key, double fallback,
                  bool required = false) const {
        const bool present = push(key);
        if ((!present && required) ||
            (present && lua_type(L, -1) != LUA_TNUMBER)) {
            throw Error(path + "." + key + " requires a number");
        }

        const double value = present ? lua_tonumber(L, -1) : fallback;
        lua_pop(L, 1);
        if (!std::isfinite(value)) {
            throw Error(path + "." + key + " must be finite");
        }

        return value;
    }

    uint32_t integer(const char *key, uint32_t fallback,
                     bool required = false) const {
        const double v = number(key, fallback, required);
        if (v < 0 || v > UINT32_MAX || v != std::floor(v)) {
            throw Error(path + "." + key + " requires a uint32 integer");
        }

        return uint32_t(v);
    }

    bool boolean(const char *key, bool fallback) const {
        const bool present = push(key);
        if (present && !lua_isboolean(L, -1)) {
            throw Error(path + "." + key + " requires a boolean");
        }

        const bool value = present ? lua_toboolean(L, -1) != 0 : fallback;
        lua_pop(L, 1);
        return value;
    }

    std::string string(const char *key, const std::string &fallback,
                       bool required = false) const {
        const bool present = push(key);
        if ((!present && required) ||
            (present && lua_type(L, -1) != LUA_TSTRING)) {
            throw Error(path + "." + key + " requires a string");
        }

        size_t length = 0;
        const char *text = present ? lua_tolstring(L, -1, &length) : nullptr;
        std::string value = present ? std::string(text, length) : fallback;
        lua_pop(L, 1);
        if (value.size() > 4096 || value.find('\0') != std::string::npos) {
            throw Error(path + "." + key +
                        " exceeds 4096 bytes or contains NUL");
        }

        return value;
    }

    template <class T>
    T enumeration(const char *key, T fallback, int count) const {
        const auto text = string(key, name(fallback));
        for (int i = 0; i < count; ++i) {
            if (text == name(static_cast<T>(i))) {
                return static_cast<T>(i);
            }
        }

        throw Error(path + "." + key + ": unknown value '" + text + "'");
    }

    template <class F> void child(const char *key, F read) const {
        if (push(key)) {
            read(Table(L, path + "." + key));
        }

        lua_pop(L, 1);
    }
};

Layer readLayer(const Table &t) {
    t.keys({"type", "enabled", "start", "duration", "gain", "pan", "phase",
            "duty", "damping", "waveform", "color", "frequency", "envelope",
            "modulation", "filter"});
    Layer l;
    t.string("type", "", true);
    l.type = t.enumeration("type", l.type, 3);
    l.waveform = t.enumeration("waveform", l.waveform, 5);
    l.color = t.enumeration("color", l.color, 3);
    l.enabled = t.boolean("enabled", l.enabled);
#define NUMBER(field) l.field = t.number(#field, l.field)
    NUMBER(start);
    NUMBER(duration);
    NUMBER(gain);
    NUMBER(pan);
    NUMBER(phase);
    NUMBER(duty);
    NUMBER(damping);
#undef NUMBER
    t.child("frequency", [&](const Table &f) {
        f.keys({"startHz", "endHz", "curve"});
        l.frequency.startHz = f.number("startHz", l.frequency.startHz);
        l.frequency.endHz = f.number("endHz", l.frequency.startHz);
        l.frequency.curve = f.enumeration("curve", l.frequency.curve, 2);
    });
    t.child("envelope", [&](const Table &e) {
        e.keys({"attack", "decay", "sustain", "release", "exponential"});
        l.envelope.attack = e.number("attack", l.envelope.attack);
        l.envelope.decay = e.number("decay", l.envelope.decay);
        l.envelope.sustain = e.number("sustain", l.envelope.sustain);
        l.envelope.release = e.number("release", l.envelope.release);
        l.envelope.exponential =
            e.boolean("exponential", l.envelope.exponential);
    });
    t.child("filter", [&](const Table &f) {
        f.keys({"type", "cutoffHz", "q"});
        l.filter.type = f.enumeration("type", l.filter.type, 4);
        l.filter.cutoffHz = f.number("cutoffHz", l.filter.cutoffHz);
        l.filter.q = f.number("q", l.filter.q);
    });
    t.child("modulation", [&](const Table &m) {
        m.keys({"rateHz", "pitchCents", "amplitude"});
        l.modulation.rateHz = m.number("rateHz", l.modulation.rateHz);
        l.modulation.pitchCents =
            m.number("pitchCents", l.modulation.pitchCents);
        l.modulation.amplitude = m.number("amplitude", l.modulation.amplitude);
    });
    return l;
}

std::string quote(const std::string &text) {
    std::string out = "\"";
    for (unsigned char c : text) {
        if (c == '\\' || c == '"') {
            out += '\\';
            out += char(c);
        } else if (c < 32 || c == 127) {
            out += '\\';
            out += char('0' + c / 100);
            out += char('0' + (c / 10) % 10);
            out += char('0' + c % 10);
        } else {
            out += char(c);
        }
    }

    return out + '"';
}

void destination(const fs::path &path, const char *extension, bool replace) {
    if (path.extension() != extension || path.filename().empty()) {
        throw Error("Output requires a " + std::string(extension) +
                    " filename: " + path.string());
    }

    if (fs::is_symlink(fs::symlink_status(path)) ||
        (fs::exists(path) && (!replace || !fs::is_regular_file(path)))) {
        throw Error("Refusing to overwrite existing/linked destination: " +
                    path.string());
    }

    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path());
    }
}

fs::path temporary(const fs::path &target) {
    static std::atomic<uint64_t> serial{0};
    auto out = target;
    out += "." +
           std::to_string(
               std::chrono::steady_clock::now().time_since_epoch().count()) +
           "-" + std::to_string(++serial) + ".tmp";
    return out;
}

void commit(const fs::path &temp, const fs::path &target, bool replace) {
#ifdef _WIN32
    const DWORD flags =
        MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
    if (!MoveFileExW(temp.c_str(), target.c_str(), flags)) {
        throw Error("Cannot publish " + target.string() + ": Windows error " +
                    std::to_string(GetLastError()));
    }
#else
    if (!replace) {
        // Atomic no-clobber publication, including concurrent exporters.
        fs::create_hard_link(temp, target);
        fs::remove(temp);
    } else {
        fs::rename(temp, target);
    }
#endif
}

template <class F>
void staged(const fs::path &path, const char *extension, bool replace,
            F write) {
    destination(path, extension, replace);
    const auto temp = temporary(path);
    try {
        write(temp);
        destination(path, extension, replace);
        commit(temp, path, replace);
    } catch (...) {
        std::error_code ignored;
        fs::remove(temp, ignored);
        throw;
    }
}
} // namespace

SoundDefinition parse(const std::string &text, const std::string &source) {
    try {
        if (text.size() > 256 * 1024) {
            throw Error("recipe exceeds 256 KiB");
        }

        Budget budget;
        std::unique_ptr<lua_State, decltype(&lua_close)> state(
            lua_newstate(allocate, &budget), lua_close);
        if (!state) {
            throw Error("cannot allocate Lua state");
        }

        auto *L = state.get();
        lua_sethook(
            L,
            [](lua_State *vm, lua_Debug *) {
                luaL_error(vm, "recipe exceeded 100000 Lua instructions");
            },
            LUA_MASKCOUNT, 100000);
        const int loaded =
            luaL_loadbufferx(L, text.data(), text.size(), source.c_str(), "t");
        const int status = loaded ? loaded : lua_pcall(L, 0, 1, 0);
        // Only bounded raw lookups below, no execution or metamethods. Leave
        // diagnostic allocation headroom outside Lua's protected call.
        budget.limit = std::numeric_limits<size_t>::max();
        if (status) {
            const char *message = lua_tostring(L, -1);
            throw Error(message ? message : "Lua load failed");
        }

        Table t(L, "sound");
        t.keys({"schemaVersion", "id", "category", "description", "sampleRate",
                "channels", "duration", "seed", "outputGainDb", "peakLimit",
                "loop", "layers"});
        SoundDefinition s;
        s.schemaVersion = t.integer("schemaVersion", 1, true);
        s.id = t.string("id", "", true);
        s.category = t.string("category", s.category);
        s.description = t.string("description", s.description);
        s.sampleRate = t.integer("sampleRate", s.sampleRate);
        s.channels = t.integer("channels", s.channels);
        s.seed = t.integer("seed", s.seed);
        s.duration = t.number("duration", s.duration, true);
        s.outputGainDb = t.number("outputGainDb", s.outputGainDb);
        s.peakLimit = t.number("peakLimit", s.peakLimit);
        t.child("loop", [&](const Table &l) {
            l.keys({"enabled", "begin", "end", "crossfade"});
            s.loop.enabled = l.boolean("enabled", true);
            s.loop.begin = l.number("begin", s.loop.begin);
            s.loop.end = l.number("end", s.loop.end);
            s.loop.crossfade = l.number("crossfade", s.loop.crossfade);
        });
        t.push("layers");
        Table layers(L, "layers");
        const size_t count = lua_rawlen(L, layers.index);
        if (!count || count > maxLayers) {
            throw Error("layers requires 1..32 entries");
        }

        // Reject sparse arrays, string keys and silently ignored extra layers.
        size_t actual = 0;
        lua_pushnil(L);
        while (lua_next(L, layers.index)) {
            const double key =
                lua_type(L, -2) == LUA_TNUMBER ? lua_tonumber(L, -2) : -1;
            if (key < 1 || key > count || key != std::floor(key)) {
                throw Error("layers requires a dense array");
            }

            ++actual;
            lua_pop(L, 1);
        }

        if (actual != count) {
            throw Error("layers requires a dense array");
        }

        s.layers.clear();
        for (size_t i = 1; i <= count; ++i) {
            lua_rawgeti(L, layers.index, lua_Integer(i));
            s.layers.push_back(
                readLayer(Table(L, "layers[" + std::to_string(i) + "]")));
            lua_pop(L, 1);
        }

        validate(s);
        return s;
    } catch (const std::exception &e) {
        throw Error(source + ": " + e.what());
    }
}

SoundDefinition load(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw Error(path.string() + ": cannot open recipe");
    }

    std::string text(256 * 1024 + 1, '\0');
    in.read(text.data(), std::streamsize(text.size()));
    if (in.bad()) {
        throw Error(path.string() + ": cannot read recipe");
    }

    text.resize(size_t(in.gcount()));
    return parse(text, path.string());
}

std::string serialize(const SoundDefinition &s) {
    validate(s);
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << std::setprecision(17) << std::boolalpha;
    o << "-- Guitar++ procedural SFX, schema 1. Times in seconds; pan "
         "-1..1.\nreturn {\n"
      << "    schemaVersion = 1,\n    id = " << quote(s.id)
      << ",\n    category = " << quote(s.category)
      << ",\n    description = " << quote(s.description)
      << ",\n    sampleRate = " << s.sampleRate
      << ",\n    channels = " << s.channels
      << ",\n    duration = " << s.duration << ",\n    seed = " << s.seed
      << ",\n    outputGainDb = " << s.outputGainDb
      << ",\n    peakLimit = " << s.peakLimit << ",\n";
    o << "    loop = { enabled = " << s.loop.enabled
      << ", begin = " << s.loop.begin << ", [\"end\"] = " << s.loop.end
      << ", crossfade = " << s.loop.crossfade << " },\n    layers = {\n";
    for (const auto &l : s.layers) {
        o << "        {\n            type = " << quote(name(l.type))
          << ", enabled = " << l.enabled << ",\n            start = " << l.start
          << ", duration = " << l.duration << ",\n            gain = " << l.gain
          << ", pan = " << l.pan << ", phase = " << l.phase
          << ", duty = " << l.duty << ", damping = " << l.damping
          << ",\n            waveform = " << quote(name(l.waveform))
          << ", color = " << quote(name(l.color))
          << ",\n            frequency = { startHz = " << l.frequency.startHz
          << ", endHz = " << l.frequency.endHz
          << ", curve = " << quote(name(l.frequency.curve))
          << " },\n            envelope = { attack = " << l.envelope.attack
          << ", decay = " << l.envelope.decay
          << ", sustain = " << l.envelope.sustain
          << ", release = " << l.envelope.release
          << ", exponential = " << l.envelope.exponential
          << " },\n            modulation = { rateHz = " << l.modulation.rateHz
          << ", pitchCents = " << l.modulation.pitchCents
          << ", amplitude = " << l.modulation.amplitude
          << " },\n            filter = { type = " << quote(name(l.filter.type))
          << ", cutoffHz = " << l.filter.cutoffHz << ", q = " << l.filter.q
          << " },\n        },\n";
    }

    return o.str() + "    },\n}\n";
}

void save(const fs::path &path, const SoundDefinition &s, bool replace) {
    const auto text = serialize(s);
    staged(path, ".lua", replace, [&](const fs::path &temp) {
        std::ofstream out(temp, std::ios::binary);
        out.write(text.data(), std::streamsize(text.size()));
        out.close();
        if (!out) {
            throw Error("Cannot write recipe: " + path.string());
        }
    });
}

void exportWav(const fs::path &path, const PcmBuffer &pcm, bool float32,
               bool replace, const Checkpoint &cb) {
    if (cb) {
        cb(0);
    }

    staged(path, ".wav", replace, [&](const fs::path &temp) {
        writeWav(temp, pcm, float32);
        if (cb) {
            cb(1); // Cancellation before publication removes the staged file.
        }
    });
}

std::vector<fs::path> recipes(const fs::path &root) {
    std::vector<fs::path> result;
    for (const auto &entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_symlink() && entry.is_regular_file() &&
            entry.path().extension() == ".lua") {
            result.push_back(entry.path());
            if (result.size() > 4096) {
                throw Error("Recipe directory exceeds 4096 files");
            }
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}
} // namespace gpp::audio::sfx
