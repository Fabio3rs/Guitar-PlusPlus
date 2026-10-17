#include "core.h"
#include "merge.h"
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

std::map<uint64_t, Projection> Session::partProjections() const {
    auto result = projections;
    if (projection) {
        result[state.partId] = *projection;
    }
    return result;
}

void Session::selectPart(uint64_t id) {
    if (id == state.partId) {
        return;
    }
    // Switching is navigation, not a document edit to undo.
    projections = partProjections();
    activatePart(state, id);
    const auto found = projections.find(id);
    projection = found == projections.end() ? std::optional<Projection>{}
                                            : found->second;
    selection.clear();
    clipboard.clear();
    ++revision;
    dirty = true;
}

void Session::addPart(const Settings &settings) {
    auto next = state;
    Part part;
    part.partId = next.nextPartId++;
    part.settings = settings;
    part.settings.offset = state.settings.offset;
    next.otherParts.push_back(part);
    validate(next);
    checkpoint();
    state = std::move(next);
    selectPart(part.partId);
}

void Session::removePart(uint64_t id) {
    if (state.otherParts.empty()) {
        throw std::runtime_error("Keep at least one part in the project");
    }
    auto next = state;
    activatePart(next, id);
    activatePart(next, next.otherParts.front().partId);
    next.merges.erase(id);
    next.otherParts.erase(
        std::remove_if(next.otherParts.begin(), next.otherParts.end(),
                       [id](const Part &part) { return part.partId == id; }),
        next.otherParts.end());
    validate(next);
    projections = partProjections();
    checkpoint();
    state = std::move(next);
    const auto found = projections.find(state.partId);
    projection = found == projections.end() ? std::optional<Projection>{}
                                            : found->second;
    selection.clear();
    clipboard.clear();
}

void Session::configurePart(const Settings &settings, bool resetSource) {
    auto next = state;
    const bool changedSource = settings.track != state.settings.track ||
                               settings.channel != state.settings.channel ||
                               settings.program != state.settings.program;
    if (changedSource && state.merges.count(state.partId)) {
        throw std::runtime_error(
            "Detach the merge before changing its primary source");
    }
    if (changedSource &&
        (!state.events.empty() || !state.plus.phrases.empty())) {
        if (!resetSource) {
            throw std::runtime_error("Changing the source requires Reset this "
                                     "part; existing edits are undoable");
        }
        Part fresh;
        fresh.partId = state.partId;
        fresh.plus.settings = state.plus.settings;
        static_cast<Part &>(next) = std::move(fresh);
    }
    next.settings = settings;
    next.settings.offset = state.settings.offset;
    validate(next);
    checkpoint();
    state = std::move(next);
    if (changedSource) {
        projection.reset();
        projections.erase(state.partId);
        selection.clear();
        clipboard.clear();
    }
}

void Session::applyGenerated(State result, const std::string &sourceHash) {
    if (hash(encode(state)) != sourceHash) {
        throw std::runtime_error(
            "Project changed; batch result discarded. Generate again.");
    }
    validate(result);
    checkpoint();
    state = std::move(result);
}

void Session::runAll(const std::vector<Pattern> &library) {
    const auto source = hash(encode(state));
    applyGenerated(generateParts(state, library, partProjections()), source);
}

void Session::setAlignment(double scale, double offset,
                           const std::string &evidence) {
    auto next = state;
    next.alignment = {scale, evidence};
    next.settings.offset = offset;
    synchronize(next);
    validate(next);
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
    projections.clear();
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
    projections.clear();
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
    if (next.merges.count(next.partId)) {
        if (region || projection) {
            throw std::runtime_error(
                "Use merge regions for composition edits; source UMAP "
                "regeneration needs separate artifacts");
        }
        generateMerged(next, library);
    } else {
        generate(next, library, region, projection ? &*projection : nullptr);
    }
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

void Session::insert(int64_t tick, unsigned int mask, int64_t duration) {
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
    if (state.merges.count(state.partId)) {
        throw std::runtime_error(
            "Use Merge tracks to regenerate the composition; clipped motifs "
            "cannot be recalculated independently");
    }
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
    projections = partProjections();
    future_.push_back(state);
    state = std::move(past_.back());
    past_.pop_back();
    dirty = true;
    selection.clear();
    clipboard.clear();
    const auto found = projections.find(state.partId);
    projection = found == projections.end() ? std::optional<Projection>{}
                                            : found->second;
    return true;
}

bool Session::redo() {
    ++revision;
    if (future_.empty()) {
        return false;
    }
    projections = partProjections();
    past_.push_back(state);
    state = std::move(future_.back());
    future_.pop_back();
    dirty = true;
    selection.clear();
    clipboard.clear();
    const auto found = projections.find(state.partId);
    projection = found == projections.end() ? std::optional<Projection>{}
                                            : found->second;
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

namespace ChartStudio {
void Session::applyMerge(const MergeProposal &proposal) {
    if (proposal.source != hash(encode(state))) {
        throw std::runtime_error("Merge preview is stale; analyze again");
    }
    validate(proposal.state);
    checkpoint();
    state = proposal.state;
    selection.clear();
}
void Session::detachMerge() {
    const auto found = state.merges.find(state.partId);
    if (found == state.merges.end()) {
        return;
    }
    auto next = state;
    // Explicitly restore the original mapped primary, not a disguised merged
    // chart that a subsequent Generate would silently overwrite.
    if (found->second.initialized) {
        static_cast<Part &>(next) = found->second.primary;
    }
    next.merges.erase(state.partId);
    next.settings.offset = state.settings.offset;
    synchronize(next);
    validate(next);
    checkpoint();
    state = std::move(next);
    selection.clear();
}
} // namespace ChartStudio
