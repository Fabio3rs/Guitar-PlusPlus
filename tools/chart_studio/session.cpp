#include "core.h"
#include "plus_generator.h"
#include <algorithm>
#include <stdexcept>

namespace ChartStudio {
namespace {
Event &find(State &state, uint64_t id) {
    for (auto &event : state.events) {
        if (event.note.id == id) {
            return event;
        }
    }
    throw std::runtime_error("No selected event");
}
void capturePattern(State &state, const Event &selected, int status) {
    if (selected.exact.empty()) {
        return;
    }
    Pattern pattern;
    pattern.exact = selected.exact;
    pattern.structural = selected.structural;
    pattern.parameters = state.settings.fingerprint();
    pattern.status = status;
    pattern.provenance = "manual";
    for (const auto &event : state.events) {
        if (event.phrase == selected.phrase) {
            pattern.masks.push_back(event.removed ? 0 : event.note.flags);
        }
    }
    state.patterns[pattern.exact] = std::move(pattern);
}
} // namespace

void Session::setAlignment(double scale, double offset,
                           const std::string &evidence) {
    auto next = state;
    next.alignment = {scale, evidence};
    next.settings.offset = offset;
    validate(next);
    synchronize(next);
    checkpoint();
    state = std::move(next);
}

void Session::checkpoint() {
    ++revision;
    past_.push_back(state);
    if (past_.size() > 64) {
        past_.erase(past_.begin());
    }
    future_.clear();
    dirty = true;
}

void Session::openMidi(const fs::path &path) {
    ++documentGeneration;
    ++revision;
    State next;
    next.score = importMidi(path);
    next.input = fs::absolute(path).u8string();
    next.inputHash = fileHash(path);
    state = std::move(next);
    projection.reset();
    project.clear();
    savedHash_.clear();
    past_.clear();
    future_.clear();
    selection.clear();
    dirty = true;
}

void Session::openProject(const fs::path &path) {
    ++documentGeneration;
    ++revision;
    auto next = loadProject(path);
    auto saved = fileHash(path);
    state = std::move(next);
    projection.reset();
    project = fs::absolute(path);
    savedHash_ = std::move(saved);
    past_.clear();
    future_.clear();
    selection.clear();
    dirty = false;
}

void Session::save(const fs::path &path) {
    const auto destination = fs::absolute(path).lexically_normal();
    const bool same =
        !project.empty() && destination == project.lexically_normal();
    if (same && fs::exists(destination) &&
        fileHash(destination) != savedHash_) {
        throw std::runtime_error(
            "Project changed on disk; save under another name");
    }
    saveProject(destination, state, same);
    project = destination;
    savedHash_ = fileHash(destination);
    dirty = false;
}

void Session::run(const std::vector<Pattern> &library,
                  std::optional<std::pair<int64_t, int64_t>> region) {
    if (!state.input.empty() && fs::exists(fs::u8path(state.input)) &&
        fileHash(fs::u8path(state.input)) != state.inputHash) {
        throw std::runtime_error(
            "Source MIDI changed; reopen it explicitly before regeneration");
    }
    auto next = state;
    generate(next, library, region, projection ? &*projection : nullptr);
    checkpoint();
    state = std::move(next);
}

void Session::edit(uint64_t id, int mask, int64_t tick, int64_t duration,
                   bool all) {
    if (tick < 0 || tick > INT32_MAX || duration < 0 || duration > INT32_MAX) {
        throw std::runtime_error("Tick and sustain must be nonnegative");
    }
    auto next = state;
    const Event selected = find(next, id);
    size_t position = 0;
    for (const auto &event : next.events) {
        if (event.note.id == id) {
            break;
        }
        if (event.phrase == selected.phrase) {
            ++position;
        }
    }
    std::map<std::string, size_t> positions;
    for (auto &event : next.events) {
        const bool equivalent = event.exact == selected.exact ||
                                (next.settings.structural &&
                                 event.structural == selected.structural);
        const size_t index = positions[event.phrase]++;
        if (event.note.id != id &&
            !(all && equivalent && !selected.exact.empty() &&
              index == position)) {
            continue;
        }
        gpp::charter::Document document;
        document.nextId = next.nextId;
        document.notes.push_back(event.note);
        document.lanes(event.note.id, mask);
        event.note = document.notes.front();
        event.tick += tick - selected.tick;
        event.duration = duration;
        event.manual = true;
        event.removed = false;
        event.origin = "manual";
        if (!all) {
            event.reason = "independent occurrence override";
        }
    }
    synchronize(next);
    validate(next);
    if (all && tick == selected.tick && duration == selected.duration) {
        capturePattern(next, find(next, id), selected.status);
    }
    checkpoint();
    state = std::move(next);
}

void Session::removeSelected() {
    auto next = state;
    for (auto &event : next.events) {
        if (selection.count(event.note.id)) {
            event.removed = true;
            event.manual = true;
            event.origin = "manual";
        }
    }
    checkpoint();
    state = std::move(next);
    selection.clear();
}

void Session::insert(int64_t tick, int mask, int64_t duration) {
    auto next = state;
    gpp::charter::Document document;
    document.nextId = next.nextId;
    Event event;
    event.note.id = document.insert({0, 0, 0, mask});
    document.lanes(event.note.id, mask);
    event.note.flags = mask;
    event.tick = tick;
    event.duration = duration;
    event.manual = true;
    event.origin = "manual";
    event.phrase = "manual-" + std::to_string(event.note.id);
    next.nextId = document.nextId;
    next.events.push_back(event);
    synchronize(next);
    validate(next);
    checkpoint();
    state = std::move(next);
    selection = {event.note.id};
}

void Session::copy() {
    clipboard.clear();
    for (const auto &event : state.events) {
        if (selection.count(event.note.id)) {
            clipboard.push_back(event);
        }
    }
}

void Session::paste(int64_t tick) {
    if (tick < 0 || tick > INT32_MAX) {
        throw std::runtime_error("Invalid paste tick");
    }
    if (clipboard.empty()) {
        return;
    }
    auto next = state;
    const auto delta = tick - clipboard.front().tick;
    std::set<uint64_t> selected;
    for (auto event : clipboard) {
        event.note.id = next.nextId++;
        event.tick += delta;
        event.sources.clear();
        event.exact.clear();
        event.structural.clear();
        event.phrase = "manual-" + std::to_string(event.note.id);
        event.manual = true;
        event.status = 0;
        event.origin = "manual";
        selected.insert(event.note.id);
        next.events.push_back(std::move(event));
    }
    synchronize(next);
    validate(next);
    checkpoint();
    state = std::move(next);
    selection = std::move(selected);
}

void Session::pattern(uint64_t id, int status, bool all) {
    if (status < 0 || status > 2) {
        throw std::runtime_error("Invalid pattern state");
    }
    auto next = state;
    const Event selected = find(next, id);
    capturePattern(next, selected, status);
    const auto pattern = next.patterns.find(selected.exact);
    std::map<std::string, size_t> positions;
    for (auto &event : next.events) {
        const auto index = positions[event.phrase]++;
        if (event.phrase == selected.phrase ||
            (all && !selected.exact.empty() &&
             (event.exact == selected.exact ||
              (next.settings.structural &&
               event.structural == selected.structural)))) {
            if (all && pattern != next.patterns.end() &&
                index < pattern->second.masks.size()) {
                event.note.flags = pattern->second.masks[index];
                event.removed = !event.note.flags;
            }
            event.status = status;
        }
    }
    next.history.push_back("Pattern " + hash(selected.exact) + " status " +
                           std::to_string(status));
    validate(next);
    checkpoint();
    state = std::move(next);
}

void Session::recalculate(uint64_t id) {
    auto next = state;
    const Event selected = find(next, id);
    if (selected.status > 0 || selected.manual) {
        throw std::runtime_error("Unlock and restore manual decisions "
                                 "explicitly before recalculating");
    }
    next.patterns.erase(selected.exact);
    for (auto &event : next.events) {
        if (event.exact == selected.exact && event.status == 0 &&
            !event.manual) {
            event.reason.clear();
        }
    }
    const bool reuse = next.settings.reuse;
    next.settings.reuse = false;
    int64_t end = selected.tick + 1;
    int64_t begin = selected.tick;
    for (const auto &event : next.events) {
        if (event.phrase == selected.phrase) {
            begin = std::min(begin, event.tick);
            end = std::max(end, event.tick + 1);
        }
    }
    generate(next, {}, {{begin, end}}, projection ? &*projection : nullptr);
    next.settings.reuse = reuse;
    checkpoint();
    state = std::move(next);
}

void Session::restore(int64_t begin, int64_t end) {
    auto next = state;
    for (auto &event : next.events) {
        if (event.tick < begin || event.tick >= end || event.status == 2) {
            continue;
        }
        const auto old = std::find_if(
            next.original.begin(), next.original.end(),
            [&](const Event &e) { return e.note.id == event.note.id; });
        if (old != next.original.end()) {
            event = *old;
        } else {
            event.removed = true;
            event.manual = true;
        }
    }
    synchronize(next);
    checkpoint();
    state = std::move(next);
}

void Session::clearExperimental() {
    auto next = state;
    for (auto it = next.patterns.begin(); it != next.patterns.end();) {
        if (it->second.status == 0) {
            it = next.patterns.erase(it);
        } else {
            ++it;
        }
    }
    checkpoint();
    state = std::move(next);
}

bool Session::undo() {
    ++revision;
    if (past_.empty()) {
        return false;
    }
    future_.push_back(state);
    state = std::move(past_.back());
    past_.pop_back();
    dirty = true;
    selection.clear();
    return true;
}

bool Session::redo() {
    ++revision;
    if (future_.empty()) {
        return false;
    }
    past_.push_back(state);
    state = std::move(future_.back());
    future_.pop_back();
    dirty = true;
    selection.clear();
    return true;
}
void Session::configurePlus(const PlusSettings &settings) {
    settings.validate();
    checkpoint();
    state.plus.settings = settings;
}

void Session::applyPlus(const PlusProposal &proposal) {
    if (proposal.source != hash(encode(state))) {
        throw std::runtime_error("Plus proposal is stale; analyze again");
    }
    State next = state;
    next.plus = proposal.document;
    validate(next);
    const auto errors = plusProblems(next);
    if (!errors.empty()) {
        throw std::runtime_error(errors.front());
    }
    checkpoint();
    state = std::move(next);
}

void Session::editPlus(PlusPhrase phrase, bool remove) {
    State next = state;
    auto &phrases = next.plus.phrases;
    auto found = std::find_if(phrases.begin(), phrases.end(),
                              [&](const auto &p) { return p.id == phrase.id; });
    if (found != phrases.end()) {
        if (found->status == 2 && (remove || found->begin != phrase.begin ||
                                   found->end != phrase.end)) {
            throw std::runtime_error("Unlock the Plus phrase before editing");
        }
        if (remove) {
            phrases.erase(found);
        } else {
            phrase.manual = true;
            *found = phrase;
        }
    } else {
        if (remove || phrase.id != 0) {
            throw std::runtime_error("Unknown Plus phrase");
        }
        phrase.id = next.plus.nextId++;
        phrase.manual = true;
        phrases.push_back(phrase);
    }
    std::sort(phrases.begin(), phrases.end(),
              [](const auto &a, const auto &b) { return a.begin < b.begin; });
    validate(next);
    checkpoint();
    state = std::move(next);
}

} // namespace ChartStudio
