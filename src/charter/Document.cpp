#include "Document.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace gpp::charter {
double advanceBeats(const std::vector<TempoPoint> &tempos, double time,
                    double beats) {
    if (!std::isfinite(time) || !std::isfinite(beats)) {
        throw std::runtime_error("Invalid beat navigation");
    }
    double previous = -std::numeric_limits<double>::infinity();
    for (const auto &tempo : tempos) {
        if (!std::isfinite(tempo.time) || !std::isfinite(tempo.bpm) ||
            tempo.bpm <= 0 || tempo.time < previous) {
            throw std::runtime_error("Invalid editor tempo map");
        }
        previous = tempo.time;
    }
    const bool forward = beats >= 0;
    double remaining = std::abs(beats);
    for (size_t step = 0; step <= tempos.size(); ++step) {
        double bpm = tempos.empty() ? 120 : tempos.front().bpm;
        double boundary = forward ? std::numeric_limits<double>::infinity()
                                  : -std::numeric_limits<double>::infinity();
        for (const auto &tempo : tempos) {
            if (tempo.time < time || (forward && tempo.time == time)) {
                bpm = tempo.bpm;
                if (!forward) {
                    boundary = tempo.time;
                }
            } else if (forward) {
                boundary = tempo.time;
                break;
            }
        }
        const double available = std::abs(boundary - time) * bpm / 60;
        if (remaining <= available || !std::isfinite(boundary)) {
            return time + (forward ? 1 : -1) * remaining * 60 / bpm;
        }
        remaining -= available;
        time = boundary;
    }
    throw std::runtime_error("Cannot navigate editor tempo map");
}

namespace {
Note &requireNote(Document &document, uint64_t id) {
    for (auto &note : document.notes) {
        if (note.id == id) {
            return note;
        }
    }
    throw std::runtime_error("Editor note no longer exists");
}
} // namespace

const Note *Document::find(uint64_t id) const {
    for (const auto &note : notes) {
        if (note.id == id) {
            return &note;
        }
    }
    return nullptr;
}

void Document::sort() {
    std::sort(notes.begin(), notes.end(), [](const Note &a, const Note &b) {
        return a.time != b.time ? a.time < b.time : a.id < b.id;
    });
}

uint64_t Document::insert(Note note) {
    if (nextId == std::numeric_limits<uint64_t>::max()) {
        throw std::runtime_error("Editor note IDs exhausted");
    }
    note.id = nextId++;
    notes.push_back(note);
    sort();
    return note.id;
}

void Document::remove(uint64_t id) {
    notes.erase(std::remove_if(notes.begin(), notes.end(),
                               [id](const Note &n) { return n.id == id; }),
                notes.end());
}

void Document::move(uint64_t id, double time) {
    requireNote(*this, id).time = time;
    sort();
}

void Document::lanes(uint64_t id, int mask) {
    if (mask < 1 || mask > 31) {
        throw std::runtime_error("Expected five-lane mask 1..31");
    }
    auto &note = requireNote(*this, id);
    note.flags = (note.flags & ~31) | mask;
}

void Document::sustain(uint64_t id, double duration) {
    requireNote(*this, id).duration = duration;
}

std::optional<uint64_t> Document::nearest(double time, double tolerance) const {
    std::optional<uint64_t> result;
    double distance = tolerance;
    for (const auto &note : notes) {
        const double delta = std::abs(note.time - time);
        if (delta <= distance) {
            result = note.id;
            distance = delta;
        }
    }
    return result;
}

void Document::validate() const {
    std::set<uint64_t> ids;
    double previous = -std::numeric_limits<double>::infinity();
    for (const auto &note : notes) {
        if (!note.id || note.id >= nextId || !ids.insert(note.id).second ||
            !std::isfinite(note.time) || !std::isfinite(note.duration) ||
            !std::isfinite(note.time + note.duration) || note.duration < 0 ||
            note.time < previous) {
            throw std::runtime_error("Invalid editor document");
        }
        previous = note.time;
    }
}

void Editor::reset(Document document) {
    document.validate();
    document_ = std::move(document);
    past_.clear();
    future_.clear();
    selection.clear();
}

void Editor::edit(const std::function<void(Document &)> &command) {
    auto next = document_;
    command(next);
    next.sort();
    next.validate();
    past_.push_back(document_);
    if (past_.size() > 64) {
        past_.erase(past_.begin());
    }
    document_ = std::move(next);
    future_.clear();
}

bool Editor::undo() {
    if (past_.empty()) {
        return false;
    }
    future_.push_back(document_);
    document_ = std::move(past_.back());
    past_.pop_back();
    selection.clear();
    return true;
}

bool Editor::redo() {
    if (future_.empty()) {
        return false;
    }
    past_.push_back(document_);
    document_ = std::move(future_.back());
    future_.pop_back();
    selection.clear();
    return true;
}
} // namespace gpp::charter
