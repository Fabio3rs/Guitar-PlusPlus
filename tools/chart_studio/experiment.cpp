#include "core.h"
#include <fstream>
#include <stdexcept>

namespace ChartStudio {
namespace {
struct ProjectionRow {
    std::string signature;
    std::vector<int> positions;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(signature), CEREAL_NVP(positions));
    }
};
struct SourceRecord {
    int pitch, velocity, channel, program;
    int64_t duration;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(pitch), CEREAL_NVP(velocity), CEREAL_NVP(channel),
          CEREAL_NVP(program), CEREAL_NVP(duration));
    }
};
struct EventRecord {
    uint64_t id;
    int64_t tick, duration;
    double seconds;
    unsigned int mask;
    int status;
    bool removed;
    std::string phrase, signature;
    std::vector<SourceRecord> sources;
    template <class A> void serialize(A &a) {
        a(CEREAL_NVP(id), CEREAL_NVP(tick), CEREAL_NVP(duration),
          CEREAL_NVP(seconds), CEREAL_NVP(mask), CEREAL_NVP(status),
          CEREAL_NVP(removed), CEREAL_NVP(phrase), CEREAL_NVP(signature),
          CEREAL_NVP(sources));
    }
};
} // namespace

Projection readProjection(const fs::path &path) {
    if (fs::file_size(path) > 32 * 1024 * 1024) {
        throw std::runtime_error("Projection file exceeds 32 MiB");
    }
    std::ifstream file(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(file)), {});
    if (!file || text.empty()) {
        throw std::runtime_error("Cannot read UMAP projection");
    }
    Projection projection;
    int schema = 0;
    std::vector<ProjectionRow> rows;
    std::istringstream input(text);
    cereal::JSONInputArchive archive(input);
    archive(CEREAL_NVP(schema),
            cereal::make_nvp("input_hash", projection.inputHash),
            cereal::make_nvp("difficulty", projection.difficulty),
            cereal::make_nvp("track", projection.track),
            cereal::make_nvp("channel", projection.channel),
            cereal::make_nvp("program", projection.program),
            cereal::make_nvp("metadata", projection.metadata),
            CEREAL_NVP(rows));
    if (schema != 1 || rows.empty() || rows.size() > 20000) {
        throw std::runtime_error("Unsupported or empty UMAP projection");
    }
    for (auto &row : rows) {
        if (row.signature.empty() || row.positions.empty() ||
            row.positions.size() > 32 ||
            !projection.positions.emplace(row.signature, row.positions)
                 .second) {
            throw std::runtime_error("Invalid or duplicate UMAP phrase");
        }
        for (int value : row.positions) {
            if (value < 0 || value > 48) {
                throw std::runtime_error("UMAP target outside 0..48");
            }
        }
    }
    projection.identity = hash(text);
    return projection;
}

std::string experimentEvents(const State &state) {
    validate(state);
    std::vector<EventRecord> events;
    events.reserve(state.events.size());
    for (const auto &event : state.events) {
        EventRecord record{event.note.id,    event.tick,
                           event.duration,   event.note.time,
                           event.note.flags, event.status,
                           event.removed,    event.phrase,
                           event.structural, {}};
        for (const auto &source : event.sources) {
            record.sources.push_back({source.pitch, source.velocity,
                                      source.channel, source.program,
                                      source.end - source.start});
        }
        events.push_back(std::move(record));
    }
    std::ostringstream output;
    {
        cereal::JSONOutputArchive archive(output);
        archive(cereal::make_nvp("schema", 1),
                cereal::make_nvp("input_hash", state.inputHash),
                cereal::make_nvp("ppq", state.score.ppq),
                cereal::make_nvp("difficulty", state.settings.difficulty),
                cereal::make_nvp("track", state.settings.track),
                cereal::make_nvp("channel", state.settings.channel),
                cereal::make_nvp("program", state.settings.program),
                CEREAL_NVP(events));
    }
    return output.str();
}
} // namespace ChartStudio
