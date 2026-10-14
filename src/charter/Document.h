#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <vector>

namespace gpp::charter {
struct TempoPoint {
    double time = 0, bpm = 120;
};
double advanceBeats(const std::vector<TempoPoint> &tempos, double time,
                    double beats);

// Editor values only. No playback cursors, input, rendering or studio metadata.
struct Note {
    uint64_t id = 0;
    double time = 0, duration = 0;
    int flags = 0;
};

struct Document {
    std::vector<Note> notes;
    uint64_t nextId = 1;
    const Note *find(uint64_t id) const;
    uint64_t insert(Note note);
    void remove(uint64_t id);
    void move(uint64_t id, double time);
    void lanes(uint64_t id, int mask);
    void sustain(uint64_t id, double duration);
    std::optional<uint64_t> nearest(double time, double tolerance) const;
    void validate() const;
    void sort();
};

// Whole gestures are transactions, not individual rendered frames.
class Editor {
    Document document_;
    std::vector<Document> past_, future_;

  public:
    std::set<uint64_t> selection;
    const Document &document() const { return document_; }
    void reset(Document document);
    void edit(const std::function<void(Document &)> &command);
    bool undo();
    bool redo();
};
} // namespace gpp::charter
