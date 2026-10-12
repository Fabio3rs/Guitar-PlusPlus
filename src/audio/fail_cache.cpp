#include "fail_cache.h"
#include <algorithm>
#include <cereal/archives/json.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <type_traits>
#ifdef _WIN32
#include <windows.h>
#endif

namespace gpp::audio {
namespace {
constexpr uint32_t schema = 1, analyzerVersion = 1, synthVersion = 1;
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}
void checkpoint(const Checkpoint &cb, double p) {
    if (cb) {
        cb(p);
    }
}
constexpr uint64_t maxManifest = 256ull * 1024 * 1024,
                   maxPcm = 512ull * 1024 * 1024;
} // namespace
template <class A> void serialize(A &ar, SourceIdentity &x) {
    ar(cereal::make_nvp("path", x.path), cereal::make_nvp("hash", x.hash),
       cereal::make_nvp("bytes", x.bytes));
}
template <class A> void serialize(A &ar, SourceInfo &x) {
    ar(cereal::make_nvp("rate", x.rate),
       cereal::make_nvp("channels", x.channels),
       cereal::make_nvp("duration", x.duration));
}
template <class A> void serialize(A &ar, Signature &x) {
    ar(cereal::make_nvp("chroma", x.chroma),
       cereal::make_nvp("bands", x.bands));
}
template <class A> void serialize(A &ar, SongSettings &x) {
    visitSongSettings(x, [&](const std::string &name, auto &v, double, double) {
        ar(cereal::make_nvp(name.c_str(), v));
    });
}
template <class A> void serialize(A &ar, Settings &x) {
    visitSettings(x, [&](const std::string &name, auto &v, double, double) {
        ar(cereal::make_nvp(name.c_str(), v));
    });
}
template <class A> void serialize(A &ar, Observation &x) {
    ar(cereal::make_nvp("frame", x.frame),
       cereal::make_nvp("signature", x.signature),
       cereal::make_nvp("rms", x.rms),
       cereal::make_nvp("variation", x.variation));
}
template <class A> void serialize(A &ar, FailRegion &x) {
    ar(cereal::make_nvp("begin", x.begin), cereal::make_nvp("end", x.end),
       cereal::make_nvp("representative", x.representative),
       cereal::make_nvp("signature", x.signature),
       cereal::make_nvp("setId", x.setId), cereal::make_nvp("silent", x.silent),
       cereal::make_nvp("change", x.change));
}
template <class A> void serialize(A &ar, Candidate &x) {
    ar(cereal::make_nvp("midi", x.midi),
       cereal::make_nvp("frequency", x.frequency),
       cereal::make_nvp("score", x.score), cereal::make_nvp("name", x.name));
}
template <class A> void serialize(A &ar, AnalysisResult &x) {
    ar(cereal::make_nvp("rate", x.rate),
       cereal::make_nvp("spectrum", x.spectrum),
       cereal::make_nvp("notes", x.notes),
       cereal::make_nvp("milliseconds", x.milliseconds));
}
template <class A> void serialize(A &ar, FailSet &x) {
    ar(cereal::make_nvp("sampleIds", x.sampleIds),
       cereal::make_nvp("representative", x.representative),
       cereal::make_nvp("analysis", x.analysis),
       cereal::make_nvp("fallback", x.fallback));
}
template <class A> void serialize(A &ar, SongAnalysis &x) {
    ar(cereal::make_nvp("source", x.source), cereal::make_nvp("info", x.info),
       cereal::make_nvp("frames", x.frames),
       cereal::make_nvp("bassVersion", x.bassVersion),
       cereal::make_nvp("settings", x.settings),
       cereal::make_nvp("observations", x.observations),
       cereal::make_nvp("regions", x.regions),
       cereal::make_nvp("detectedRegions", x.detectedRegions),
       cereal::make_nvp("identifyMs", x.identifyMs),
       cereal::make_nvp("openMs", x.openMs),
       cereal::make_nvp("decodeMs", x.decodeMs),
       cereal::make_nvp("fftMs", x.fftMs),
       cereal::make_nvp("descriptorMs", x.descriptorMs),
       cereal::make_nvp("scanMs", x.scanMs),
       cereal::make_nvp("segmentMs", x.segmentMs));
}
namespace {
struct SampleMeta {
    unsigned rate = 0, channels = 0;
    uint64_t frames = 0, loopBegin = 0, loopEnd = 0, hash = 0;
    double contentSeconds = 0;
    template <class A> void serialize(A &ar) {
        ar(CEREAL_NVP(rate), CEREAL_NVP(channels), CEREAL_NVP(frames),
           CEREAL_NVP(loopBegin), CEREAL_NVP(loopEnd), CEREAL_NVP(hash),
           CEREAL_NVP(contentSeconds));
    }
};
uint64_t pcmHash(const PcmBuffer &pcm) {
    uint64_t hash = 14695981039346656037ull;
    for (float v : pcm.samples) {
        uint32_t bits;
        std::memcpy(&bits, &v, 4);
        for (unsigned i = 0; i < 4; ++i) {
            hash = (hash ^ uint8_t(bits >> (8 * i))) * 1099511628211ull;
        }
    }
    return hash;
}
struct Manifest {
    uint32_t version = schema, analyzer = analyzerVersion, synth = synthVersion;
    SongAnalysis song;
    Settings settings;
    uint32_t fxVersion = 0;
    std::vector<FailSet> sets;
    std::vector<SampleMeta> samples;
    double representativeMs = 0, synthMs = 0, fxMs = 0;
    template <class A> void serialize(A &ar) {
        ar(CEREAL_NVP(version), CEREAL_NVP(analyzer), CEREAL_NVP(synth),
           CEREAL_NVP(song), CEREAL_NVP(settings), CEREAL_NVP(fxVersion),
           CEREAL_NVP(sets), CEREAL_NVP(samples), CEREAL_NVP(representativeMs),
           CEREAL_NVP(synthMs), CEREAL_NVP(fxMs));
    }
};
std::string json(const Manifest &m) {
    std::ostringstream text;
    {
        cereal::JSONOutputArchive ar(text);
        ar(cereal::make_nvp("cache", m));
    }
    return text.str();
}
// Reuse the field declarations above, but check types before RapidJSON's typed
// accessors. Cereal's JSON input archive otherwise asserts on malformed types.
class CheckedInput {
    const rapidjson::Value &node_;
    template <class T>
    static void read(T &value, const rapidjson::Value &node) {
        if constexpr (std::is_same_v<T, bool>) {
            if (!node.IsBool()) {
                throw Error("Expected cache boolean");
            }
            value = node.GetBool();
        } else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
            if (!node.IsUint64() ||
                node.GetUint64() > std::numeric_limits<T>::max()) {
                throw Error("Expected cache unsigned integer");
            }
            value = T(node.GetUint64());
        } else if constexpr (std::is_integral_v<T>) {
            if (!node.IsInt64() ||
                node.GetInt64() < std::numeric_limits<T>::min() ||
                node.GetInt64() > std::numeric_limits<T>::max()) {
                throw Error("Expected cache integer");
            }
            value = T(node.GetInt64());
        } else if constexpr (std::is_floating_point_v<T>) {
            if (!node.IsNumber() || !std::isfinite(node.GetDouble()) ||
                std::abs(node.GetDouble()) > std::numeric_limits<T>::max()) {
                throw Error("Expected finite cache number");
            }
            value = T(node.GetDouble());
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (!node.IsString() ||
                std::strlen(node.GetString()) != node.GetStringLength()) {
                throw Error("Expected cache string without NUL");
            }
            value.assign(node.GetString(), node.GetStringLength());
        } else {
            CheckedInput nested(node);
            if constexpr (std::is_same_v<T, Manifest> ||
                          std::is_same_v<T, SampleMeta>) {
                value.serialize(nested);
            } else {
                serialize(nested, value);
            }
        }
    }
    template <class T>
    static void read(std::vector<T> &values, const rapidjson::Value &node) {
        if (!node.IsArray() || node.Size() > 2000000) {
            throw Error("Invalid cache array");
        }
        values.resize(node.Size());
        for (size_t i = 0; i < values.size(); ++i) {
            read(values[i], node[rapidjson::SizeType(i)]);
        }
    }
    template <class T, size_t N>
    static void read(std::array<T, N> &values, const rapidjson::Value &node) {
        if (!node.IsObject() || node.MemberCount() != N) {
            throw Error("Invalid cache fixed array");
        }
        for (size_t i = 0; i < N; ++i) {
            const auto key = "value" + std::to_string(i);
            if (!node.HasMember(key.c_str())) {
                throw Error("Missing cache array element");
            }
            read(values[i], node[key.c_str()]);
        }
    }
    template <class T> void field(const cereal::NameValuePair<T> &pair) {
        if (!node_.HasMember(pair.name)) {
            throw Error(std::string("Missing cache field: ") + pair.name);
        }
        read(pair.value, node_[pair.name]);
    }

  public:
    explicit CheckedInput(const rapidjson::Value &node) : node_(node) {
        if (!node.IsObject()) {
            throw Error("Expected cache object");
        }
    }
    template <class... Args> void operator()(Args &&...args) {
        (field(args), ...);
    }
};
Manifest parse(const std::string &text) {
    if (text.size() > maxManifest) {
        throw Error("Cache manifest exceeds size limit");
    }
    rapidjson::Document document;
    document.Parse<rapidjson::kParseValidateEncodingFlag>(text.c_str(),
                                                          text.size());
    if (document.HasParseError()) {
        throw Error("Malformed cache JSON");
    }
    Manifest m;
    CheckedInput ar(document);
    ar(cereal::make_nvp("cache", m));
    if (m.version != schema || m.analyzer != analyzerVersion ||
        m.synth != synthVersion) {
        throw Error("Unsupported cache processing version; regenerate");
    }
    validate(m.song);
    validate(m.settings);
    return m;
}
void safeWrite(const std::filesystem::path &path, const SourceIdentity &source,
               const std::function<void(std::ostream &)> &write,
               const Checkpoint &cb) {
    if (std::filesystem::weakly_canonical(path) ==
            std::filesystem::weakly_canonical(source.path) ||
        (std::filesystem::exists(path) &&
         std::filesystem::exists(source.path) &&
         std::filesystem::equivalent(path, source.path))) {
        throw Error("Cannot overwrite audio source");
    }
    std::filesystem::path scratch;
    for (int i = 0; i < 100; ++i) {
        auto candidate = path;
        candidate += ".tmp-" +
                     std::to_string(Clock::now().time_since_epoch().count()) +
                     "-" + std::to_string(i);
        if (std::filesystem::create_directory(candidate)) {
            scratch = candidate;
            break;
        }
    }
    if (scratch.empty()) {
        throw Error("Cannot reserve cache temporary directory");
    }
    struct Cleanup {
        std::filesystem::path p;
        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove_all(p, ec);
        }
    } cleanup{scratch};
    const auto temp = scratch / "cache";
    std::ofstream out(temp, std::ios::binary);
    write(out);
    out.close();
    if (!out) {
        throw Error("Cannot write cache");
    }
    checkpoint(cb, 1); // Commit point: cancellation after this point may
                       // observe a committed file.
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw Error("Cannot replace cache");
    }
#else
    std::filesystem::rename(temp, path);
#endif
}
void writeInt(std::ostream &out, uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) {
        out.put(char(uint8_t(value >> (i * 8))));
    }
}
uint64_t readInt(std::istream &in, unsigned bytes) {
    uint64_t value = 0;
    for (unsigned i = 0; i < bytes; ++i) {
        const int c = in.get();
        if (c == EOF) {
            throw Error("Truncated cache");
        }
        value |= uint64_t(uint8_t(c)) << (8 * i);
    }
    return value;
}
Manifest manifest(const SongFailCache &c) {
    Manifest m;
    m.song = c.song;
    m.song.observations.clear();
    m.settings = c.settings;
    m.fxVersion = c.fxVersion;
    m.sets = c.sets;
    // Synthesis consumes the candidates, not the display spectrum. Keep the
    // bank compact.
    for (auto &set : m.sets) {
        set.analysis.spectrum.clear();
    }
    m.representativeMs = c.representativeMs;
    m.synthMs = c.synthMs;
    m.fxMs = c.fxMs;
    for (const auto &s : c.samples) {
        m.samples.push_back({s.pcm.rate, s.pcm.channels, s.pcm.frames(),
                             s.pcm.loop.begin, s.pcm.loop.end, pcmHash(s.pcm),
                             s.contentSeconds});
    }
    return m;
}
} // namespace

Settings shortFailSettings() {
    Settings s;
    s.synth.duration = .051;
    return s;
}

bool sourceMatches(const SourceIdentity &identity,
                   const std::filesystem::path &path, const Checkpoint &cb) {
    const auto actual = identifySource(path, cb);
    return actual.bytes == identity.bytes && actual.hash == identity.hash;
}

const FailRegion *SongFailCache::regionAt(double seconds) const {
    if (!std::isfinite(seconds) || song.regions.empty()) {
        return nullptr;
    }
    if (seconds <= 0) {
        return &song.regions.front();
    }
    if (seconds >= song.info.duration) {
        return &song.regions.back();
    }
    const auto frame = uint64_t(seconds * song.info.rate);
    const auto it =
        std::upper_bound(song.regions.begin(), song.regions.end(), frame,
                         [](uint64_t f, const auto &r) { return f < r.begin; });
    return &*(it == song.regions.begin() ? it : std::prev(it));
}

uint64_t SongFailCache::pcmBytes() const {
    uint64_t result = 0;
    for (const auto &s : samples) {
        result += s.pcm.samples.size() * sizeof(float);
    }
    return result;
}

SongFailCache generateFailCache(const SongAnalysis &song,
                                const Settings &settings,
                                const Checkpoint &cb) {
    validate(song);
    validate(settings);
    if (song.bassVersion != BASS_GetVersion() ||
        !sourceMatches(song.source, song.source.path,
                       [&](double p) { checkpoint(cb, p * .05); })) {
        throw Error("Stale analysis; scan source again");
    }
    SongFailCache out;
    out.song = song;
    out.settings = settings;
    out.fxVersion = BASS_FX_GetVersion();
    uint32_t count = 0;
    for (const auto &r : song.regions) {
        count = std::max(count, r.setId + 1);
    }
    if (count > song.regions.size()) {
        throw Error("Invalid set IDs");
    }
    for (uint32_t id = 0; id < count; ++id) {
        const auto it =
            std::find_if(song.regions.begin(), song.regions.end(),
                         [&](const auto &r) { return r.setId == id; });
        if (it == song.regions.end()) {
            throw Error("Missing set anchor");
        }
        FailSet set;
        set.representative = it->representative;
        set.fallback = it->silent;
        auto report = [&](double p) {
            checkpoint(cb, .05 + .9 * (id + p) / count);
        };
        auto t = Clock::now();
        if (!it->silent) {
            set.analysis = analyze(
                song.source.path, double(set.representative) / song.info.rate,
                settings.analysis, [&](double) { report(0); });
        }
        set.fallback = set.analysis.notes.empty();
        out.representativeMs += elapsed(t);
        for (int fret = 0; fret < 5; ++fret) {
            auto synth = settings.synth;
            synth.fretMask = 1 << fret;
            t = Clock::now();
            auto raw = synthesize(set.analysis, synth,
                                  [&](double) { report(double(fret) / 5); });
            out.synthMs += elapsed(t);
            t = Clock::now();
            auto processed = processFx(
                raw, settings.fx, [&](double) { report(double(fret) / 5); });
            bakeLoop(processed, synth.crossfade);
            out.fxMs += elapsed(t);
            set.sampleIds[size_t(fret)] = uint32_t(out.samples.size());
            out.samples.push_back({std::move(processed), raw.seconds()});
            if (out.pcmBytes() > maxPcm) {
                throw Error("Bank exceeds 512 MiB PCM limit; reduce duration "
                            "or sensitivity");
            }
        }
        out.sets.push_back(std::move(set));
    }
    if (!sourceMatches(song.source, song.source.path,
                       [&](double p) { checkpoint(cb, .95 + .05 * p); })) {
        throw Error("Audio changed during generation");
    }
    validate(out);
    checkpoint(cb, 1);
    return out;
}

void validate(const SongFailCache &c) {
    validate(c.song);
    validate(c.settings);
    if (c.sets.empty() || c.samples.empty() ||
        c.samples.size() > c.song.regions.size() * 5 || c.pcmBytes() > maxPcm) {
        throw Error("Invalid cache bank size");
    }
    for (const auto &r : c.song.regions) {
        if (r.setId >= c.sets.size()) {
            throw Error("Invalid cache set ID");
        }
    }
    for (const auto &s : c.sets) {
        if (s.representative >= c.song.frames) {
            throw Error("Invalid cache representative");
        }
        for (auto id : s.sampleIds) {
            if (id >= c.samples.size()) {
                throw Error("Invalid cache sample ID");
            }
        }
    }
    for (const auto &s : c.samples) {
        if (s.pcm.samples.empty() || !std::isfinite(s.contentSeconds) ||
            s.contentSeconds <= 0) {
            throw Error("Invalid cached sample");
        }
        levels(s.pcm);
    }
}

void saveSongAnalysis(const std::filesystem::path &path,
                      const SongAnalysis &song, const Checkpoint &cb) {
    validate(song);
    Manifest m;
    m.song = song;
    safeWrite(
        path, song.source,
        [&](std::ostream &out) {
            checkpoint(cb, 0);
            out << json(m);
        },
        cb);
}

SongAnalysis loadSongAnalysis(const std::filesystem::path &path,
                              const std::filesystem::path &source,
                              const Checkpoint &cb) {
    if (std::filesystem::file_size(path) > maxManifest) {
        throw Error("Analysis JSON exceeds size limit");
    }
    std::ifstream in(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(in)), {});
    auto m = parse(text);
    if (m.song.observations.empty() ||
        m.song.bassVersion != BASS_GetVersion() ||
        !sourceMatches(m.song.source, source, cb)) {
        throw Error("Stale analysis; scan again");
    }
    m.song.source.path = std::filesystem::absolute(source).string();
    return m.song;
}

void saveFailCache(const std::filesystem::path &path,
                   const SongFailCache &cache, const Checkpoint &cb) {
    validate(cache);
    const auto text = json(manifest(cache));
    safeWrite(
        path, cache.song.source,
        [&](std::ostream &out) {
            out.write("GPPFAIL1", 8);
            writeInt(out, text.size(), 8);
            out.write(text.data(), std::streamsize(text.size()));
            for (size_t i = 0; i < cache.samples.size(); ++i) {
                const auto &pcm = cache.samples[i].pcm;
                for (size_t j = 0; j < pcm.samples.size(); ++j) {
                    if (j % 4096 == 0) {
                        checkpoint(cb, (i + double(j) / pcm.samples.size()) /
                                           cache.samples.size());
                    }
                    uint32_t bits;
                    std::memcpy(&bits, &pcm.samples[j], 4);
                    writeInt(out, bits, 4);
                }
            }
        },
        cb);
}

SongFailCache loadFailCache(const std::filesystem::path &path,
                            const std::filesystem::path &source,
                            const Checkpoint &cb) {
    const auto fileSize = std::filesystem::file_size(path);
    std::ifstream in(path, std::ios::binary);
    char magic[8]{};
    in.read(magic, 8);
    if (!in || std::memcmp(magic, "GPPFAIL1", 8) != 0) {
        throw Error("Not a Guitar++ fail cache");
    }
    const auto length = readInt(in, 8);
    if (length > maxManifest || fileSize < 16 || length > fileSize - 16) {
        throw Error("Invalid cache manifest length");
    }
    std::string text(size_t(length), '\0');
    in.read(text.data(), std::streamsize(length));
    auto m = parse(text);
    if (m.song.bassVersion != BASS_GetVersion() ||
        m.fxVersion != BASS_FX_GetVersion() ||
        !sourceMatches(m.song.source, source,
                       [&](double p) { checkpoint(cb, p * .1); })) {
        throw Error("Stale fail cache; regenerate");
    }
    uint64_t bytes = 0;
    for (const auto &s : m.samples) {
        if (s.rate < 8000 || s.rate > 192000 || s.channels < 1 ||
            s.channels > 8 || s.frames > maxPcm / 4 / s.channels ||
            (s.loopEnd && (s.loopBegin >= s.loopEnd || s.loopEnd > s.frames))) {
            throw Error("Invalid cache sample format");
        }
        bytes += s.frames * s.channels * 4;
        if (bytes > maxPcm) {
            throw Error("Cache PCM exceeds size limit");
        }
    }
    if (bytes != fileSize - 16 - length) {
        throw Error("Truncated or extended cache payload");
    }
    SongFailCache c;
    c.song = std::move(m.song);
    c.song.source.path = std::filesystem::absolute(source).string();
    c.settings = m.settings;
    c.fxVersion = m.fxVersion;
    c.sets = std::move(m.sets);
    c.representativeMs = m.representativeMs;
    c.synthMs = m.synthMs;
    c.fxMs = m.fxMs;
    for (size_t i = 0; i < m.samples.size(); ++i) {
        const auto &meta = m.samples[i];
        FailSample sample;
        sample.contentSeconds = meta.contentSeconds;
        sample.pcm.rate = meta.rate;
        sample.pcm.channels = meta.channels;
        sample.pcm.loop = {size_t(meta.loopBegin), size_t(meta.loopEnd)};
        sample.pcm.samples.resize(size_t(meta.frames * meta.channels));
        for (size_t j = 0; j < sample.pcm.samples.size(); ++j) {
            if (j % 4096 == 0) {
                checkpoint(
                    cb, .1 + .9 * (i + double(j) / sample.pcm.samples.size()) /
                                 m.samples.size());
            }
            uint32_t bits = uint32_t(readInt(in, 4));
            std::memcpy(&sample.pcm.samples[j], &bits, 4);
        }
        if (pcmHash(sample.pcm) != meta.hash) {
            throw Error("Cache PCM checksum mismatch");
        }
        c.samples.push_back(std::move(sample));
    }
    validate(c);
    checkpoint(cb, 1);
    return c;
}

std::string songReportJson(const SongAnalysis &song, bool observations) {
    Manifest m;
    m.song = song;
    if (!observations) {
        m.song.observations.clear();
    }
    return json(m);
}
std::string cacheReportJson(const SongFailCache &cache) {
    return json(manifest(cache));
}
} // namespace gpp::audio
