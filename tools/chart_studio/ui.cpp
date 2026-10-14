#include "audio/audio.h"
#include "core.h"
#include "runtime.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>
#include <iostream>
#include <memory>
#include <png.h>

namespace ChartStudio {
namespace {
const ImU32 colors[] = {IM_COL32(55, 210, 110, 255), IM_COL32(240, 75, 85, 255),
                        IM_COL32(245, 205, 65, 255),
                        IM_COL32(80, 155, 250, 255),
                        IM_COL32(245, 140, 55, 255)};
struct Workbench {
    Session session;
    std::string inputPath, projectPath, outputPath, libraryPath, referencePath,
        audioPath;
    std::string instrument = "[ExpertSingle]", message;
    Reference reference;
    gpp::audio::AnalysisResult spectrum;
    bool loopRegion = false;
    double cursor = 0, start = 0, scale = 90, referenceOffset = 0;
    bool playing = false, propagate = false, replaceOutput = false;
    int64_t regionBegin = 0, regionEnd = 0;
    uint64_t dragging = 0;
    float dragX = 0;
    Event dragEvent;
    std::function<void()> pending;
    std::unique_ptr<gpp::audio::BassSystem> system;
    std::unique_ptr<gpp::audio::AudioTransport> transport;
    std::chrono::steady_clock::time_point last =
        std::chrono::steady_clock::now();
    int audioDevice = -1;

    explicit Workbench(const fs::path &output, int device)
        : audioDevice(device) {
        projectPath = (output / "untitled.gppchartproj").u8string();
        outputPath = (output / "untitled.generated.chart").u8string();
        libraryPath = (output / "patterns.sqlite").u8string();
    }
    void perform(const std::function<void()> &action) {
        try {
            action();
            message.clear();
        } catch (const std::exception &error) {
            message = error.what();
        }
    }
    void request(const std::function<void()> &action) {
        if (session.dirty) {
            pending = action;
            ImGui::OpenPopup("Unsaved project");
        } else {
            perform(action);
        }
    }
    void open(const fs::path &path) {
        if (path.extension() == ".gppchartproj") {
            session.openProject(path);
            projectPath = path.u8string();
            inputPath = session.state.input;
            referencePath = session.state.reference;
            audioPath = session.state.audio;
        } else {
            session.openMidi(path);
            inputPath = path.u8string();
        }
        playing = false;
        cursor = 0;
        start = 0;
        reference = {};
        transport.reset();
        system.reset();
    }
    Event *selected() {
        if (session.selection.empty()) {
            return nullptr;
        }
        for (auto &event : session.state.events) {
            if (event.note.id == *session.selection.begin()) {
                return &event;
            }
        }
        return nullptr;
    }
    void tick() {
        const auto now = std::chrono::steady_clock::now();
        const double delta = std::chrono::duration<double>(now - last).count();
        last = now;
        if (playing) {
            if (transport && BASS_ChannelIsActive(transport->handle()) ==
                                 BASS_ACTIVE_PLAYING) {
                cursor = transport->position();
            } else {
                cursor += delta;
            }
            if (loopRegion && regionEnd > regionBegin &&
                cursor >= session.state.score.seconds(regionEnd) +
                              session.state.settings.offset) {
                seek(session.state.score.seconds(regionBegin) +
                     session.state.settings.offset);
                if (transport) {
                    transport->play();
                }
            }
        }
    }
    void seek(double time) {
        cursor = std::max(0.0, time);
        if (transport) {
            transport->position(cursor);
        }
    }
    void playback() {
        playing = !playing;
        if (transport) {
            if (playing) {
                transport->position(cursor);
                transport->play();
            } else {
                transport->pause();
            }
        }
        last = std::chrono::steady_clock::now();
    }
    void exportChart() {
        protectDestination(fs::u8path(outputPath),
                           {session.project, fs::u8path(projectPath),
                            fs::u8path(libraryPath)});
        exportValidated(session.state, fs::u8path(outputPath), replaceOutput);
    }
    void files() {
        if (!ImGui::CollapsingHeader("Files and project",
                                     ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }
        ImGui::InputText("MIDI / project", &inputPath);
        if (ImGui::Button("Open")) {
            request([this] { open(fs::u8path(inputPath)); });
        }
        ImGui::SameLine();
        if (ImGui::Button("Save project")) {
            perform([&] { session.save(fs::u8path(projectPath)); });
        }
        ImGui::InputText("Project", &projectPath);
        ImGui::InputText("Chart output", &outputPath);
        ImGui::Checkbox("Replace output explicitly", &replaceOutput);
        if (ImGui::Button("Export .chart")) {
            perform([&] { exportChart(); });
        }
        ImGui::InputText("Pattern library", &libraryPath);
        ImGui::InputText("Reference", &referencePath);
        ImGui::InputText("Reference part", &instrument);
        if (ImGui::Button("Load reference")) {
            perform([&] {
                reference =
                    readReference(fs::u8path(referencePath), instrument);
                session.state.reference =
                    fs::absolute(fs::u8path(referencePath)).u8string();
                session.dirty = true;
            });
        }
        ImGui::InputDouble("Reference shift (s)", &referenceOffset, .01);
        ImGui::InputText("Optional audio", &audioPath);
        if (ImGui::Button("Load audio")) {
            perform([&] {
                if (!system) {
                    gpp::audio::DeviceSettings s;
                    s.device = audioDevice;
                    system = std::make_unique<gpp::audio::BassSystem>(s);
                }
                auto next = std::make_unique<gpp::audio::AudioTransport>();
                next->open(fs::u8path(audioPath));
                transport = std::move(next);
                playing = false;
                session.state.audio =
                    fs::absolute(fs::u8path(audioPath)).u8string();
                session.dirty = true;
            });
        }
        if (ImGui::Button("FFT at playback position")) {
            perform([&] {
                if (!system) {
                    gpp::audio::DeviceSettings s;
                    s.device = audioDevice;
                    system = std::make_unique<gpp::audio::BassSystem>(s);
                }
                spectrum =
                    gpp::audio::analyze(fs::u8path(audioPath), cursor, {});
            });
        }
        if (!spectrum.spectrum.empty()) {
            ImGui::PlotLines("FFT", spectrum.spectrum.data(),
                             int(spectrum.spectrum.size()), 0, nullptr, 0,
                             FLT_MAX, {0, 70});
            ImGui::TextWrapped("%.1f Hz/bin; mixed audio candidates (harmonics "
                               "may resemble notes):",
                               double(spectrum.rate) / 8192);
            for (const auto &note : spectrum.notes) {
                ImGui::Text("%s  %.1f Hz  %.3f", note.name.c_str(),
                            note.frequency, note.score);
            }
        }
        if (!session.state.input.empty() &&
            !fs::exists(fs::u8path(session.state.input))) {
            ImGui::TextWrapped(
                "Source is missing. Saved notes remain editable; reopen the "
                "MIDI to replace the source.");
        }
    }
    void tracks() {
        if (!ImGui::CollapsingHeader("Musical tracks",
                                     ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }
        for (size_t i = 0; i < session.state.score.tracks.size(); ++i) {
            const auto &track = session.state.score.tracks[i];
            const std::string label = std::to_string(i) + ": " + track.name +
                                      " (" +
                                      std::to_string(track.notes.size()) + ")";
            if (ImGui::Selectable(label.c_str(),
                                  session.state.settings.track == int(i))) {
                if (!session.state.events.empty() &&
                    session.state.settings.track != int(i)) {
                    request([this, i] {
                        session.state.events.clear();
                        session.state.original.clear();
                        session.state.patterns.clear();
                        session.state.settings.track = int(i);
                        session.state.settings.channel = -1;
                        session.state.settings.program = -1;
                        session.dirty = true;
                    });
                } else {
                    session.state.settings.track = int(i);
                    session.dirty = true;
                }
            }
        }
        if (ImGui::TreeNode("Track details (zero-based channels/programs)")) {
            const auto details = inspect(session.state.score);
            ImGui::TextUnformatted(details.c_str());
            ImGui::TreePop();
        }
    }
    void algorithm() {
        if (!ImGui::CollapsingHeader("Mapping",
                                     ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }
        auto &s = session.state.settings;
        const char *difficulties[] = {"expert", "hard", "medium", "easy"};
        int difficulty = 0;
        for (int i = 0; i < 4; ++i) {
            if (s.difficulty == difficulties[i]) {
                difficulty = i;
            }
        }
        if (ImGui::Combo("Difficulty", &difficulty, difficulties, 4)) {
            s.difficulty = difficulties[difficulty];
            session.dirty = true;
        }
        if (difficulty) {
            ImGui::TextWrapped("Experimental reduction profile");
        }
        int strategy = s.algorithm == "beam" ? 0 : 1;
        if (ImGui::Combo("Algorithm", &strategy, "Beam search\0Heuristic\0")) {
            s.algorithm = strategy ? "heuristic" : "beam";
            session.dirty = true;
        }
        session.dirty |= ImGui::InputInt("Channel (-1: all)", &s.channel);
        session.dirty |= ImGui::InputInt("Program (-1: all)", &s.program);
        session.dirty |= ImGui::SliderInt("Max chord lanes", &s.maxChord, 1, 3);
        session.dirty |= ImGui::InputDouble("Retention 0..1", &s.density, .05);
        session.dirty |= ImGui::Checkbox("Reuse cache", &s.reuse);
        session.dirty |= ImGui::Checkbox("Strict transposition", &s.structural);
        double offset = s.offset * 1000;
        if (ImGui::InputDouble("Audio offset (ms)", &offset, 10)) {
            perform([&] {
                auto settings = s;
                settings.offset = offset / 1000;
                settings.validate();
                s = settings;
                synchronize(session.state);
                session.dirty = true;
            });
        }
        bool automaticSustains = s.minimumSustain < 0;
        if (ImGui::Checkbox("Musical sustain cutoff (beats)",
                            &automaticSustains)) {
            s.minimumSustain = automaticSustains ? -.5 : 0;
            session.dirty = true;
        }
        if (automaticSustains) {
            double beats = -s.minimumSustain;
            if (ImGui::InputDouble("Sustain must exceed (beats)", &beats,
                                   .125)) {
                perform([&] {
                    if (!std::isfinite(beats) || beats <= 0 || beats > 128) {
                        throw std::runtime_error(
                            "Sustain beats must be greater than zero and at "
                            "most 128");
                    }
                    s.minimumSustain = -beats;
                    session.dirty = true;
                });
            }
        } else {
            double minimum = s.minimumSustain;
            if (ImGui::InputDouble("Min export sustain (s)", &minimum, .01)) {
                perform([&] {
                    if (minimum < 0) {
                        throw std::runtime_error(
                            "Minimum sustain seconds must be nonnegative");
                    }
                    auto settings = s;
                    settings.minimumSustain = minimum;
                    settings.validate();
                    s = settings;
                    session.dirty = true;
                });
            }
        }
        if (ImGui::TreeNode("Cost weights")) {
            const char *labels[] = {"Contour",           "Movement",
                                    "Ergonomics",        "Chord complexity",
                                    "Motif consistency", "Lane repetition"};
            for (int i = 0; i < 6; ++i) {
                session.dirty |=
                    ImGui::SliderInt(labels[i], &s.weights[i], 0, 100);
            }
            ImGui::TreePop();
        }
        if (ImGui::Button("Generate")) {
            perform([&] { session.run(loadLibrary(fs::u8path(libraryPath))); });
        }
        ImGui::InputScalar("Region begin tick", ImGuiDataType_S64,
                           &regionBegin);
        ImGui::InputScalar("Region end tick", ImGuiDataType_S64, &regionEnd);
        if (ImGui::Button("Regenerate region")) {
            perform([&] {
                if (regionBegin < 0 || regionEnd <= regionBegin) {
                    throw std::runtime_error("Select a nonempty tick region");
                }
                session.run(loadLibrary(fs::u8path(libraryPath)),
                            {{regionBegin, regionEnd}});
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Restore region")) {
            perform([&] { session.restore(regionBegin, regionEnd); });
        }
    }
    void editing() {
        if (!ImGui::CollapsingHeader("Edit and teach",
                                     ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }
        if (ImGui::Button("Undo")) {
            session.undo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Redo")) {
            session.redo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy")) {
            session.copy();
        }
        ImGui::SameLine();
        if (ImGui::Button("Paste")) {
            perform([&] {
                session.paste(session.state.score.ticks(
                    std::max(0.0, cursor - session.state.settings.offset)));
            });
        }
        if (ImGui::Button("Insert at cursor")) {
            perform([&] {
                session.insert(
                    session.state.score.ticks(
                        std::max(0.0, cursor - session.state.settings.offset)),
                    1, 0);
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete selected")) {
            session.removeSelected();
        }
        ImGui::Checkbox("Apply to equivalent occurrences", &propagate);
        if (const auto *event = selected()) {
            const Event e = *event;
            ImGui::Text("Event %llu | %zu sources | cost %lld",
                        static_cast<unsigned long long>(e.note.id),
                        e.sources.size(), static_cast<long long>(e.cost));
            int mask = e.note.flags;
            int64_t tick = e.tick, duration = e.duration;
            bool edited = false;
            for (int lane = 0; lane < 5; ++lane) {
                if (lane) {
                    ImGui::SameLine();
                }
                bool enabled = mask & (1 << lane);
                ImGui::PushID(lane);
                if (ImGui::Checkbox("", &enabled)) {
                    mask ^= 1 << lane;
                    edited = true;
                }
                ImGui::PopID();
            }
            edited |= ImGui::InputScalar("Tick", ImGuiDataType_S64, &tick);
            edited |= ImGui::InputScalar("Sustain ticks", ImGuiDataType_S64,
                                         &duration);
            if (edited) {
                perform([&] {
                    session.edit(e.note.id, mask, tick, duration, propagate);
                });
            }
            ImGui::TextWrapped(
                "Phrase %s\nPattern %s\nOrigin: %s\nMatch: %s\nState: %s",
                e.phrase.c_str(), hash(e.exact).c_str(), e.origin.c_str(),
                e.reason.c_str(),
                e.status == 2   ? "locked"
                : e.status == 1 ? "confirmed"
                                : "experimental");
            if (ImGui::Button("Confirm")) {
                perform([&] { session.pattern(e.note.id, 1, propagate); });
            }
            ImGui::SameLine();
            if (ImGui::Button("Lock")) {
                perform([&] { session.pattern(e.note.id, 2, propagate); });
            }
            ImGui::SameLine();
            if (ImGui::Button("Unlock")) {
                perform([&] { session.pattern(e.note.id, 0, propagate); });
            }
            if (ImGui::Button("Recalculate pattern")) {
                perform([&] { session.recalculate(e.note.id); });
            }
            if (ImGui::Button("Promote confirmed pattern")) {
                perform([&] {
                    promote(fs::u8path(libraryPath),
                            session.state.patterns.at(e.exact));
                });
            }
            if (ImGui::TreeNode("Occurrences")) {
                std::set<std::string> seen;
                for (const auto &occurrence : session.state.events) {
                    if (occurrence.structural == e.structural &&
                        seen.insert(occurrence.phrase).second) {
                        const auto label = "Phrase " + occurrence.phrase +
                                           " at " +
                                           std::to_string(occurrence.note.time);
                        if (ImGui::Selectable(label.c_str())) {
                            session.selection = {occurrence.note.id};
                            start = std::max(0.0, occurrence.note.time - 1);
                        }
                    }
                }
                ImGui::TreePop();
            }
        }
        if (ImGui::Button("Clear experimental cache")) {
            session.clearExperimental();
        }
    }
    void canvas() {
        ImGui::BeginChild("timeline", {0, -115}, true);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 size = ImGui::GetContentRegionAvail();
        const float piano = std::max(90.f, size.y * .38f), laneTop = piano + 30,
                    laneHeight = std::max(15.f, size.y * .042f);
        auto *draw = ImGui::GetWindowDrawList();
        ImGui::InvisibleButton("canvas", size,
                               ImGuiButtonFlags_MouseButtonLeft |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        const bool hovered = ImGui::IsItemHovered();
        auto &io = ImGui::GetIO();
        const auto x = [&](double t) {
            return origin.x + float((t - start) * scale);
        };
        if (hovered && io.MouseWheel != 0) {
            const double focus = start + (io.MousePos.x - origin.x) / scale;
            scale =
                std::clamp(scale * std::pow(1.2, io.MouseWheel), 5.0, 1600.0);
            start = std::max(0.0, focus - (io.MousePos.x - origin.x) / scale);
        }
        if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            start = std::max(0.0, start - io.MouseDelta.x / scale);
        }
        draw->AddText(origin, IM_COL32(180, 190, 210, 255),
                      "SCORE / piano roll  |  wheel: zoom, middle drag: pan");
        const auto &score = session.state.score;
        const double offset = session.state.settings.offset;
        const double end = start + size.x / scale;
        if (!score.tempos.empty()) {
            int64_t beginTick = score.ticks(std::max(0.0, start - offset));
            const int64_t endTick = score.ticks(std::max(0.0, end - offset));
            beginTick = beginTick / score.ppq * score.ppq;
            for (int64_t tick = beginTick;
                 tick <= endTick &&
                 tick - beginTick < int64_t(score.ppq) * 1000;
                 tick += score.ppq) {
                const float xx = x(score.seconds(tick) + offset);
                Signature meter;
                for (const auto &signature : score.signatures) {
                    if (signature.tick <= tick) {
                        meter = signature;
                    } else {
                        break;
                    }
                }
                const int64_t barTicks = std::max<int64_t>(
                    1, 4LL * score.ppq * meter.numerator / meter.denominator);
                const bool bar = (tick - meter.tick) % barTicks == 0;
                draw->AddLine({xx, origin.y + 20}, {xx, origin.y + size.y},
                              bar ? IM_COL32(70, 90, 115, 255)
                                  : IM_COL32(35, 45, 60, 255));
                if (bar) {
                    draw->AddText({xx + 2, origin.y + 20},
                                  IM_COL32(145, 160, 185, 255),
                                  (std::to_string(meter.numerator) + "/" +
                                   std::to_string(meter.denominator) + " @" +
                                   std::to_string(tick))
                                      .c_str());
                }
            }
            for (const auto &tempo : score.tempos) {
                const float xx = x(score.seconds(tempo.tick) + offset);
                if (xx >= origin.x && xx <= origin.x + size.x) {
                    draw->AddText(
                        {xx + 3, origin.y + piano - 16},
                        IM_COL32(235, 195, 95, 255),
                        (std::to_string(int(60000000.0 / tempo.microseconds)) +
                         " BPM")
                            .c_str());
                }
            }
        }
        std::set<uint64_t> sourceSelection;
        for (const auto &event : session.state.events) {
            if (session.selection.count(event.note.id)) {
                for (const auto &s : event.sources) {
                    sourceSelection.insert(s.id);
                }
            }
        }
        const int track = session.state.settings.track;
        if (track >= 0 && size_t(track) < score.tracks.size()) {
            for (const auto &note : score.tracks[track].notes) {
                const double at = score.seconds(note.start) + offset,
                             finish = score.seconds(note.end) + offset;
                if (at > end || finish < start) {
                    continue;
                }
                const float yy = origin.y + 42 +
                                 float(127 - note.pitch) * (piano - 50) / 128;
                draw->AddRectFilled({x(at), yy},
                                    {std::max(x(at) + 3, x(finish)), yy + 4},
                                    sourceSelection.count(note.id)
                                        ? IM_COL32(255, 235, 115, 255)
                                        : IM_COL32(80, 150, 190, 200));
            }
        }
        for (int lane = 0; lane < 5; ++lane) {
            const float yy =
                origin.y + laneTop + static_cast<float>(lane) * laneHeight;
            draw->AddLine({origin.x, yy}, {origin.x + size.x, yy},
                          colors[lane]);
        }
        uint64_t hit = 0;
        for (const auto &event : session.state.events) {
            if (event.removed || event.note.time > end ||
                event.note.time + exportDurationSeconds(session.state, event) <
                    start) {
                continue;
            }
            const float xx = x(event.note.time);
            for (int lane = 0; lane < 5; ++lane) {
                if (!(event.note.flags & (1 << lane))) {
                    continue;
                }
                const float yy =
                    origin.y + laneTop + static_cast<float>(lane) * laneHeight;
                draw->AddRectFilled(
                    {xx, yy - 2},
                    {std::max(xx + 1,
                              x(event.note.time +
                                exportDurationSeconds(session.state, event))),
                     yy + 2},
                    colors[lane]);
                draw->AddRectFilled({xx - 6, yy - 2}, {xx + 6, yy + 2},
                                    colors[lane]);
                draw->AddRectFilled({xx - 2, yy - 6}, {xx + 2, yy + 6},
                                    colors[lane]);
                if (session.selection.count(event.note.id)) {
                    draw->AddRect({xx - 8, yy - 8}, {xx + 8, yy + 8},
                                  IM_COL32_WHITE);
                }
                if (hovered && std::abs(io.MousePos.x - xx) <= 9 &&
                    std::abs(io.MousePos.y - yy) <= 10) {
                    hit = event.note.id;
                }
            }
        }
        const float referenceTop = origin.y + laneTop + laneHeight * 6;
        draw->AddText({origin.x, referenceTop - 17},
                      IM_COL32(160, 170, 195, 255),
                      "REFERENCE (read-only, arrangement may differ)");
        for (const auto &note : reference.notes) {
            const double at = note.time + referenceOffset;
            if (at > end || at + note.duration < start) {
                continue;
            }
            for (int lane = 0; lane < 5; ++lane) {
                if (note.flags & (1 << lane)) {
                    const float yy =
                        referenceTop + static_cast<float>(lane) * 14;
                    draw->AddRectFilled(
                        {x(at) - 3, yy},
                        {std::max(x(at) + 3, x(at + note.duration)), yy + 5},
                        colors[lane]);
                }
            }
        }
        for (double marker : session.state.markers) {
            if (marker >= start && marker <= end) {
                draw->AddLine({x(marker), origin.y},
                              {x(marker), origin.y + size.y},
                              IM_COL32(200, 130, 230, 180));
            }
        }
        draw->AddLine({x(cursor), origin.y}, {x(cursor), origin.y + size.y},
                      IM_COL32(255, 255, 255, 210), 2);
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (hit) {
                if (!io.KeyCtrl) {
                    session.selection.clear();
                }
                session.selection.insert(hit);
                dragging = hit;
                dragX = io.MousePos.x;
                for (const auto &event : session.state.events) {
                    if (event.note.id == hit) {
                        dragEvent = event;
                        break;
                    }
                }
            } else {
                perform(
                    [&] { seek(start + (io.MousePos.x - origin.x) / scale); });
            }
        }
        if (dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            if (std::abs(io.MousePos.x - dragX) > 4) {
                const double delta = (io.MousePos.x - dragX) / scale;
                perform([&] {
                    const auto tick = score.ticks(
                        std::max(0.0, score.seconds(dragEvent.tick) + delta));
                    const auto duration =
                        io.KeyShift
                            ? std::max<int64_t>(0, tick - dragEvent.tick +
                                                       dragEvent.duration)
                            : dragEvent.duration;
                    session.edit(dragging, dragEvent.note.flags,
                                 io.KeyShift ? dragEvent.tick : tick, duration,
                                 propagate);
                });
            }
            dragging = 0;
        }
        ImGui::EndChild();
        if (ImGui::Button(playing ? "Pause" : "Play")) {
            perform([&] { playback(); });
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            perform([&] {
                playing = false;
                if (transport) {
                    transport->pause();
                }
                seek(0);
            });
        }
        ImGui::SameLine();
        ImGui::Checkbox("Loop tick region", &loopRegion);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160);
        double position = cursor;
        if (ImGui::InputDouble("Position (s)", &position, .1)) {
            perform([&] { seek(position); });
        }
        if (ImGui::Button("Add reference marker")) {
            session.state.markers.push_back(cursor);
            session.dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Export comparison.json")) {
            perform([&] {
                safeWrite(fs::u8path(outputPath).parent_path() /
                              "comparison.json",
                          compare(asReference(session.state), reference, 0,
                                  referenceOffset),
                          replaceOutput);
            });
        }
        ImGui::TextWrapped("%s", report(session.state).c_str());
    }
    bool frame(GLFWwindow *window) {
        tick();
        auto &io = ImGui::GetIO();
        if (!io.WantTextInput) {
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
                perform([&] { session.save(fs::u8path(projectPath)); });
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
                session.undo();
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
                session.redo();
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G)) {
                perform(
                    [&] { session.run(loadLibrary(fs::u8path(libraryPath))); });
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_E)) {
                perform([&] { exportChart(); });
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q)) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
                session.copy();
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
                perform([&] {
                    session.paste(session.state.score.ticks(
                        std::max(0.0, cursor - session.state.settings.offset)));
                });
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                session.removeSelected();
            }
            if (const auto *current = selected()) {
                const Event e = *current;
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L)) {
                    perform([&] { session.pattern(e.note.id, 2, propagate); });
                }
                for (int lane = 0; lane < 5; ++lane) {
                    if (!io.KeyCtrl &&
                        ImGui::IsKeyPressed(ImGuiKey(int(ImGuiKey_1) + lane))) {
                        perform([&] {
                            session.edit(e.note.id, e.note.flags ^ (1 << lane),
                                         e.tick, e.duration, propagate);
                        });
                    }
                }
            }
        }
        const auto *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("Guitar++ Chart Studio", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings);
        bool close = false;
        if (glfwWindowShouldClose(window)) {
            glfwSetWindowShouldClose(window, GLFW_FALSE);
            if (session.dirty) {
                ImGui::OpenPopup("Close unsaved project");
            } else {
                close = true;
            }
        }
        if (ImGui::BeginPopupModal("Close unsaved project", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Save changes before closing?");
            if (ImGui::Button("Save and close")) {
                perform([&] {
                    session.save(fs::u8path(projectPath));
                    close = true;
                });
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard and close")) {
                close = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::TextUnformatted("CHART STUDIO   /   score -> five lanes -> "
                               "remembered musical decisions");
        if (!message.empty()) {
            ImGui::TextWrapped("%s", message.c_str());
        }
        if (!session.state.conflicts.empty()) {
            ImGui::TextWrapped("Conflict: %s",
                               session.state.conflicts.front().c_str());
        }
        ImGui::BeginChild("inspector", {370, 0}, true);
        files();
        tracks();
        algorithm();
        editing();
        if (ImGui::BeginPopupModal("Unsaved project", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Discard unsaved changes before opening "
                                   "another document/source?");
            if (ImGui::Button("Discard and open")) {
                if (pending) {
                    perform(pending);
                    pending = {};
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Keep editing")) {
                pending = {};
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("work", {0, 0});
        canvas();
        ImGui::EndChild();
        ImGui::End();
        return close;
    }
};

void capture(const fs::path &path, GLFWwindow *window) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    std::vector<unsigned char> pixels(size_t(width) * size_t(height) * 4),
        flipped(pixels.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const size_t stride = size_t(width) * 4;
    for (int y = 0; y < height; ++y) {
        std::copy_n(pixels.data() + size_t(height - y - 1) * stride, stride,
                    flipped.data() + size_t(y) * stride);
    }
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path());
    }
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = unsigned(width);
    image.height = unsigned(height);
    image.format = PNG_FORMAT_RGBA;
    if (!png_image_write_to_file(&image, path.u8string().c_str(), 0,
                                 flipped.data(), 0, nullptr)) {
        const std::string error = image.message;
        png_image_free(&image);
        throw std::runtime_error(error);
    }
    png_image_free(&image);
}
} // namespace
} // namespace ChartStudio

int main(int argc, char **argv) {
    using namespace ChartStudio;
    GLFWwindow *window = nullptr;
    bool context = false, glfwBackend = false, glBackend = false;
    int result = 0;
    std::string ini;
    try {
        fs::path input, screenshot, output = "chart-studio";
        int frames = 0, track = -1, device = -1;
        bool generateNow = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            auto value = [&]() -> std::string {
                if (++i == argc) {
                    throw std::runtime_error("Missing " + arg);
                }
                return argv[i];
            };
            if (arg == "--help") {
                std::cout << "gpp_chart_studio [MIDI|PROJECT] [--track N "
                             "--generate] [--output-dir DIR]\n"
                             "  [--frames N --ui-capture FILE.png] "
                             "[--audio-device N]\n";
                return 0;
            } else if (arg == "--track") {
                track = std::stoi(value());
            } else if (arg == "--generate") {
                generateNow = true;
            } else if (arg == "--output-dir") {
                output = fs::u8path(value());
            } else if (arg == "--frames") {
                frames = std::stoi(value());
                if (frames < 1) {
                    throw std::runtime_error("Frames must be positive");
                }
            } else if (arg == "--ui-capture") {
                screenshot = fs::u8path(value());
            } else if (arg == "--audio-device") {
                device = std::stoi(value());
            } else if (arg.rfind("--", 0) == 0) {
                throw std::runtime_error("Unknown argument " + arg);
            } else if (input.empty()) {
                input = fs::u8path(arg);
            } else {
                throw std::runtime_error("Only one input is accepted");
            }
        }
        if (!screenshot.empty() && !frames) {
            frames = 5;
        }
        fs::create_directories(output);
        if (!glfwInit()) {
            throw std::runtime_error(
                "Cannot initialize GLFW; use Xvfb on headless Linux");
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        window = glfwCreateWindow(1440, 960, "Guitar++ Chart Studio", nullptr,
                                  nullptr);
        if (!window) {
            throw std::runtime_error("Cannot create OpenGL 2.0 window");
        }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        context = true;
        ini = (output / "layout.ini").u8string();
        ImGui::GetIO().IniFilename = ini.c_str();
        ImGui::StyleColorsDark();
        glfwBackend = ImGui_ImplGlfw_InitForOpenGL(window, true);
        glBackend = glfwBackend && ImGui_ImplOpenGL3_Init("#version 110");
        if (!glBackend || !ImGui_ImplOpenGL3_CreateDeviceObjects()) {
            throw std::runtime_error("Cannot initialize ImGui renderer");
        }
        {
            Workbench studio(output, device);
            if (!input.empty()) {
                studio.open(input);
            }
            if (track >= 0) {
                studio.session.state.settings.track = track;
            }
            if (generateNow) {
                studio.session.run(loadLibrary(fs::u8path(studio.libraryPath)));
            }
            for (int frame = 0;; ++frame) {
                glfwPollEvents();
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();
                const bool close = studio.frame(window);
                ImGui::Render();
                int width, height;
                glfwGetFramebufferSize(window, &width, &height);
                glViewport(0, 0, width, height);
                glClearColor(.04f, .055f, .08f, 1);
                glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                const bool last = frames && frame + 1 >= frames;
                if (last && !screenshot.empty()) {
                    capture(screenshot, window);
                }
                glfwSwapBuffers(window);
                if (close || last) {
                    break;
                }
            }
        }
    } catch (const std::exception &error) {
        std::cerr << "Chart Studio: " << error.what() << '\n';
        result = 1;
    }
    if (glBackend) {
        ImGui_ImplOpenGL3_Shutdown();
    }
    if (glfwBackend) {
        ImGui_ImplGlfw_Shutdown();
    }
    if (context) {
        ImGui::DestroyContext();
    }
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
    return result;
}
