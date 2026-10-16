#pragma once
#include "charter/Document.h"
#include <array>
#include <cereal/archives/json.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "plus_types.h"
#include "synth_types.h"

namespace gpp::charter {
template <class A> void serialize(A &a, Note &n) {
    a(n.id, n.time, n.duration, n.flags);
}
} // namespace gpp::charter

namespace ChartStudio {
namespace fs = std::filesystem;
struct Tempo {
    int64_t tick = 0;
    int microseconds = 500000;
    template <class A> void serialize(A &a) { a(tick, microseconds); }
};
struct Signature {
    int64_t tick = 0;
    int numerator = 4, denominator = 4;
    template <class A> void serialize(A &a) { a(tick, numerator, denominator); }
};
struct ScoreNote {
    uint64_t id = 0;
    int pitch = 0, velocity = 0, channel = 0, program = 0;
    int64_t start = 0, end = 0;
    template <class A> void serialize(A &a) {
        a(id, pitch, velocity, channel, program, start, end);
    }
};
struct Track {
    std::string name;
    std::vector<ScoreNote> notes;
    template <class A> void serialize(A &a) { a(name, notes); }
};
struct Score {
    std::vector<MusicalMarker>
        markers; // Versioned top-level project extension.
    int ppq = 480;
    std::vector<Track> tracks;
    std::vector<Tempo> tempos{{}};
    std::vector<Signature> signatures{{}};
    std::vector<std::string> warnings;
    double seconds(int64_t tick) const;
    int64_t ticks(double seconds) const;
    void validate() const;
    template <class A> void serialize(A &a) {
        a(ppq, tracks, tempos, signatures, warnings);
    }
};
struct Settings {
    // Stored in the versioned parts extension; legacy positional fields stay
    // intact.
    std::string instrument = "guitar";
    std::string difficulty = "expert", algorithm = "beam";
    int track = -1, channel = -1, program = -1, maxChord = 3, beam = 64;
    bool structural = true, reuse = true;
    // offset is mirrored across parts for legacy engine compatibility;
    // change project timing through Session::setAlignment / synchronize.
    double offset = 0, minimumSustain = -.5, density = 1;
    // Integer cost weights, stable across platforms and locales.
    std::array<int, 6> weights{{8, 2, 4, 3, 8, 1}};
    void validate() const;
    std::string fingerprint() const;
    template <class A> void serialize(A &a) {
        a(difficulty, algorithm, track, channel, program, maxChord, beam,
          structural, reuse, offset, minimumSustain, density, weights);
    }
};
struct Event {
    gpp::charter::Note note;
    int64_t tick = 0, duration = 0;
    std::vector<ScoreNote> sources;
    std::string phrase, exact, structural, origin = "algorithm", reason;
    int status = 0; // experimental, confirmed, locked
    bool manual = false, removed = false;
    int64_t cost = 0;
    template <class A> void serialize(A &a) {
        a(note, tick, duration, sources, phrase, exact, structural, origin,
          reason, status, manual, removed, cost);
    }
};
struct Pattern {
    std::string exact, structural, parameters, provenance = "algorithm";
    std::vector<int> masks;
    int status = 0;
    template <class A> void serialize(A &a) {
        a(exact, structural, parameters, provenance, masks, status);
    }
};
struct TimingAlignment {
    double scale = 1;
    std::string evidence;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(scale), CEREAL_NVP(evidence));
    }
};
struct PlusProposal;
struct MergeProposal;
// The active part is exposed as the State base for the existing CPU engines.
// Inactive parts own only editing data; score, audio and alignment exist once.
struct Part {
    uint64_t partId = 1;
    PlusDocument plus;
    Settings settings;
    std::string reference;
    std::vector<Event> events, original;
    std::map<std::string, Pattern> patterns;
    std::vector<std::string> conflicts, history;
    uint64_t nextId = 1;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(partId), CEREAL_NVP(settings),
          cereal::make_nvp("instrument", settings.instrument), CEREAL_NVP(plus),
          CEREAL_NVP(reference), CEREAL_NVP(events), CEREAL_NVP(original),
          CEREAL_NVP(patterns), CEREAL_NVP(conflicts), CEREAL_NVP(history),
          CEREAL_NVP(nextId));
    }
};
#include "merge_types.h"
struct State : Part {
    SynthConfig synth;
    std::map<uint64_t, MergeDocument> merges;
    TimingAlignment alignment;
    Score score;
    std::string input, inputHash, audio;
    std::vector<double> markers;
    std::vector<Part> otherParts;
    uint64_t nextPartId = 2;
    template <class A> void serialize(A &a) {
        a(score, settings, input, inputHash, audio, reference, events, original,
          patterns, conflicts, history, markers, nextId);
    }
};

// Optional, external UMAP artifact. No Python dependency in the CPU library.
struct Projection {
    std::string identity, inputHash, difficulty, metadata;
    int track = -1, channel = -1, program = -1;
    std::map<std::string, std::vector<int>> positions;
};
Projection readProjection(const fs::path &path);
std::string partSection(const Settings &settings);
Settings parsePart(const std::string &association,
                   const Settings &defaults = {});
void setPartSection(Settings &settings, const std::string &section);
std::vector<const Part *> parts(const State &state);
void activatePart(State &state, uint64_t id);
State partState(const State &state, uint64_t id);
uint64_t findPart(const State &state, const std::string &section);
using PartProgress = std::function<void(size_t, size_t, const std::string &)>;
State generateParts(State state, const std::vector<Pattern> &library = {},
                    const std::map<uint64_t, Projection> &projections = {},
                    const PartProgress &progress = {});
std::string partsReport(const State &state, bool plus = false, int trials = 256,
                        uint64_t seed = 42);
std::string experimentEvents(const State &state);

std::string hash(const std::string &bytes);
std::string fileHash(const fs::path &path);
Score importMidi(const fs::path &path);
std::string inspect(const Score &score);
std::string encode(const State &state);
State decode(const std::string &text);
std::string canonical(const std::vector<Event> &events, size_t begin,
                      size_t count, const Score &score,
                      const Settings &settings, bool relative);
void generate(State &state, const std::vector<Pattern> &library = {},
              std::optional<std::pair<int64_t, int64_t>> region = {},
              const Projection *projection = nullptr);
// Negative minimumSustain encodes a beat limit (absolute value);
// nonnegative values retain the original seconds policy and project format.
int64_t exportDuration(const State &state, const Event &event);
// Separate an exclusive Plus end from the following attack when one tick
// can be removed without changing phrase membership.
int64_t exportPlusEnd(const State &state, const PlusPhrase &phrase);
double exportDurationSeconds(const State &state, const Event &event);
std::string chartText(const State &state);
std::string singleChartText(const State &state);
std::string report(const State &state);
double audioSeconds(const State &state, int64_t tick);
int64_t audioTick(const State &state, double time);
int64_t exportMilliBpm(const State &state, const Tempo &tempo);
void synchronize(State &state);
void validate(const State &state);

// SQLite project/cache I/O is studio-only. No database types leak into runtime.
void saveProject(const fs::path &path, const State &state, bool replace);
State loadProject(const fs::path &path);
std::vector<Pattern> loadLibrary(const fs::path &path);
void promote(const fs::path &path, const Pattern &pattern);
void protectDestination(const fs::path &path,
                        const std::vector<fs::path> &inputs);
void safeWrite(const fs::path &path, const std::string &text, bool replace);

class Session {
    std::vector<State> past_, future_;
    std::string savedHash_;
    void checkpoint();

  public:
    State state;
    std::map<uint64_t, Projection> projections;
    std::optional<Projection>
        projection; // Reload explicitly after opening a project.
    fs::path project;
    bool dirty = false;
    uint64_t revision = 0, documentGeneration = 0;
    std::set<uint64_t> selection;
    std::vector<Event> clipboard;
    void selectPart(uint64_t id);
    void addPart(const Settings &settings);
    void removePart(uint64_t id);
    void configurePart(const Settings &settings, bool resetSource = false);
    void applyGenerated(State result, const std::string &sourceHash);
    std::map<uint64_t, Projection> partProjections() const;
    void runAll(const std::vector<Pattern> &library = {});
    void openMidi(const fs::path &path);
    void openProject(const fs::path &path);
    void save(const fs::path &path);
    void run(const std::vector<Pattern> &library = {},
             std::optional<std::pair<int64_t, int64_t>> region = {});
    void edit(uint64_t id, int mask, int64_t tick, int64_t duration, bool all);
    void removeSelected();
    void insert(int64_t tick, int mask, int64_t duration);
    void copy();
    void paste(int64_t tick);
    void pattern(uint64_t id, int status, bool all);
    void recalculate(uint64_t id);
    void restore(int64_t begin, int64_t end);
    void clearExperimental();
    void setAlignment(double scale, double offset, const std::string &evidence);
    void configurePlus(const PlusSettings &settings);
    void applyPlus(const PlusProposal &proposal);
    void applyMerge(const MergeProposal &proposal);
    void detachMerge();
    void editPlus(PlusPhrase phrase, bool remove = false);
    bool undo();
    bool redo();
};
} // namespace ChartStudio
