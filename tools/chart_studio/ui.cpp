#include "alignment.h"
#include "audio/audio.h"
#include "core.h"
#include "file_browser.h"
#include "plus_simulation.h"
#include "runtime.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
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
    FileBrowser browser;
    bool firstLayout = true, hasSavedLayout = false, resetLayout = false,
         showHelp = false;
    bool showFiles = true, showTracks = true, showMapping = true;
    bool showEditing = true, showAlignment = true, showPlus = true;
    bool pendingPrompt = false;
    std::string notice;
    const char *helpTitle = "Getting started";
    static constexpr const char *workflowHelp =
        "Open a musical MIDI in Files, then assign game parts to musical "
        "tracks in Tracks. "
        "Generate all parts maps each source to its own five-button chart "
        "section. Review each part in the timeline "
        "and use Edit and patterns to correct or lock phrases. Save project "
        "preserves editing data; Export .chart writes the playable file. Audio "
        "and reference charts are optional.\n\nDrag panel tabs to rearrange "
        "them inside this window. Panels > Reset layout restores the "
        "default.\n\nShortcuts: Ctrl+S save, Ctrl+G generate, Ctrl+E export, "
        "Ctrl+Z/Y undo/redo, Ctrl+C/V copy/paste, Ctrl+L lock, Delete remove. "
        "Keys 1-5 toggle lanes on the selected note.";
    const char *helpText = workflowHelp;

    void help(const char *title, const char *text) {
        ImGui::PushID(title);
        if (ImGui::SmallButton("?")) {
            helpTitle = title;
            helpText = text;
            showHelp = true;
        }
        hint(title);
        ImGui::SameLine();
        ImGui::TextUnformatted(title);
        ImGui::PopID();
    }

    static void hint(const char *text) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::SetTooltip("%s", text);
        }
    }

    void pathField(const char *label, std::string &value,
                   std::vector<std::string> extensions, bool save = false) {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SetNextItemWidth(
            std::max(80.f, ImGui::GetContentRegionAvail().x - 80));
        ImGui::InputText("##path", &value);
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            browser.choose(value, std::move(extensions), save);
        }
        ImGui::PopID();
    }

    bool hasTrack() const {
        const auto track = session.state.settings.track;
        return track >= 0 && size_t(track) < session.state.score.tracks.size();
    }

    void generate() {
        if (generationJob.valid()) {
            throw std::runtime_error(
                "Wait for or cancel batch generation first");
        }
        session.run(loadLibrary(fs::u8path(libraryPath)));
        notice = "Generated " + std::to_string(session.state.events.size()) +
                 " attacks. Review the timeline, then Save project and Export "
                 ".chart.";
    }

    void saveProject() {
        session.save(fs::u8path(projectPath));
        notice = "Project saved: " + projectPath;
    }

    std::future<State> generationJob;
    std::atomic<bool> cancelGeneration{false};
    std::atomic<size_t> generatedParts{0}, generationTotal{0};
    std::string generationSource;
    int addInstrument = 1, addDifficulty = 0;
    std::optional<Settings> sourceDraft;
    uint64_t sourceDraftPart = 0;

    void generateAll() {
        if (generationJob.valid()) {
            throw std::runtime_error("Batch generation is already running");
        }
        const auto library = loadLibrary(fs::u8path(libraryPath));
        const auto snapshot = session.state;
        const auto projections = session.partProjections();
        generationSource = hash(encode(snapshot));
        cancelGeneration = false;
        generatedParts = 0;
        generationTotal = snapshot.otherParts.size() + 1;
        generationJob = std::async(
            std::launch::async, [this, snapshot, library, projections] {
                return generateParts(
                    snapshot, library, projections,
                    [this](size_t done, size_t total, const std::string &) {
                        if (cancelGeneration.load()) {
                            throw std::runtime_error(
                                "Batch cancelled; previous parts preserved");
                        }
                        generatedParts = done;
                        generationTotal = total;
                    });
            });
    }

    void changedPart() {
        dragging = 0;
        sourceDraft.reset();
        reference = {};
        referencePath = session.state.reference;
        instrument = partSection(session.state.settings);
        projectionPath.clear();
        plusLoaded.clear();
    }

    void batchControls() {
        if (generationJob.valid()) {
            ImGui::Text("Generating parts: %zu / %zu", generatedParts.load(),
                        generationTotal.load());
            if (ImGui::Button("Cancel batch")) {
                cancelGeneration = true;
            }
            hint("Cancellation is checked between parts. Previous results stay "
                 "intact.");
        } else {
            ImGui::BeginDisabled(session.state.score.tracks.empty());
            if (ImGui::Button("Generate all parts")) {
                perform([&] { generateAll(); });
            }
            ImGui::EndDisabled();
        }
        hint("Uses each part's mapping and Plus settings; applies the complete "
             "batch as one undoable change.");
    }

    std::string inputPath, projectPath, outputPath, libraryPath, referencePath,
        audioPath;
    std::string instrument = "[ExpertSingle]", message;
    std::string projectionPath;
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
    std::future<AlignmentResult> alignmentJob;
    std::atomic<bool> cancelAlignment{false};
    std::optional<AlignmentResult> alignmentResult;
    AlignmentSettings alignmentSettings;
    int alignmentCandidate = 0, alignmentMode = 2;
    bool previewAlignment = false;
    PlusSettings plusDraft;
    struct PlusJobResult {
        PlusProposal proposal;
        PlusSimulation sequence;
    };
    std::future<PlusJobResult> plusJob;
    std::atomic<bool> cancelPlus{false};
    std::optional<PlusJobResult> plusResult;
    uint64_t plusRevision = 0;
    PlusPhrase plusEdit;
    int plusPolicy = 0, plusSkill = 0;
    bool plusRegionOnly = false, showPlusOverlay = true;
    std::string plusLoaded;
    ~Workbench() {
        cancelGeneration = true;
        if (generationJob.valid()) {
            generationJob.wait();
        }
        cancelPlus = true;
        if (plusJob.valid()) {
            plusJob.wait();
        }
        stopAlignment();
    }
    void stopAlignment() {
        cancelAlignment = true;
        if (alignmentJob.valid()) {
            alignmentJob.wait();
        }
    }

    explicit Workbench(const fs::path &output, int device)
        : audioDevice(device) {
        hasSavedLayout = fs::exists(output / "layout-docking.ini");
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
            pendingPrompt = true;
        } else {
            perform(action);
        }
    }
    void open(const fs::path &path) {
        cancelGeneration = true;
        stopAlignment();
        alignmentJob = {};
        alignmentResult.reset();
        previewAlignment = false;
        if (path.extension() == ".gppchartproj") {
            session.openProject(path);
            projectPath = path.u8string();
            inputPath = session.state.input;
            referencePath = session.state.reference;
            audioPath = session.state.audio;
            if (!session.state.alignment.evidence.empty()) {
                alignmentResult =
                    decodeAlignment(session.state.alignment.evidence);
                for (size_t i = 0; i < alignmentResult->candidates.size();
                     ++i) {
                    const auto &c = alignmentResult->candidates[i];
                    if (c.scale == session.state.alignment.scale &&
                        c.offset == session.state.settings.offset) {
                        alignmentCandidate = int(i);
                    }
                }
            }
        } else {
            session.openMidi(path);
            inputPath = path.u8string();
            projectPath = (fs::u8path(projectPath).parent_path() /
                           (path.stem().u8string() + ".gppchartproj"))
                              .u8string();
            outputPath = (fs::u8path(outputPath).parent_path() /
                          (path.stem().u8string() + ".generated.chart"))
                             .u8string();
            audioPath.clear();
            referencePath.clear();
        }
        if (path.extension() == ".gppchartproj") {
            outputPath = (path.parent_path() /
                          (path.stem().u8string() + ".generated.chart"))
                             .u8string();
            notice = "Project reopened: " +
                     std::to_string(session.state.events.size()) +
                     " saved attacks. Continue editing or export. Use Load "
                     "audio / reference to enable saved external files.";
        } else {
            notice =
                "Loaded " + path.filename().u8string() + ". " +
                std::to_string(session.state.score.tracks.size()) +
                " musical tracks. Choose a track in Tracks before generating.";
        }
        playing = false;
        cursor = 0;
        start = 0;
        changedPart();
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
                cursor >= audioSeconds(session.state, regionEnd)) {
                seek(audioSeconds(session.state, regionBegin));
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
        notice = "Exported and validated with the game parser: " + outputPath;
    }
    void files() {
        help("Opening and saving",
             "Open a musical MIDI exported by TuxGuitar or another score "
             "editor. Gameplay MIDI uses different conventions and is not a "
             "musical score. Browse only fills a path: click Open MIDI / "
             "project to load it.\n\nAfter opening MIDI, choose a track in "
             "Tracks. Nothing is generated automatically. Save project stores "
             "notes, manual decisions, source references and alignment; it "
             "does not create the playable chart. Export .chart creates that "
             "separate game file.\n\nAn existing project can be reopened from "
             "the same input field. Optional audio and reference charts can be "
             "loaded below. Files used as sources are never overwritten by "
             "export.");
        ImGui::TextWrapped("1. Browse for a musical MIDI, then click Open.");
        pathField("Musical MIDI / project", inputPath,
                  {".mid", ".midi", ".gppchartproj"});
        if (ImGui::Button("Open MIDI / project")) {
            request([this] { open(fs::u8path(inputPath)); });
        }
        ImGui::Separator();
        ImGui::TextWrapped(
            "Save project keeps editable notes, source links and mapping "
            "decisions. Export creates the file used by the game.");
        ImGui::BeginDisabled(session.state.input.empty());
        if (ImGui::Button("Save project")) {
            perform([&] { saveProject(); });
        }
        ImGui::EndDisabled();
        pathField("Project", projectPath, {".gppchartproj"}, true);
        pathField("Chart output", outputPath, {".chart"}, true);
        ImGui::Checkbox("Allow replacing an existing export", &replaceOutput);
        hint("Only enable this when you intend to replace the destination "
             "chart or comparison report. Source files remain protected.");
        ImGui::BeginDisabled(session.state.events.empty());
        if (ImGui::Button("Export .chart")) {
            perform([&] { exportChart(); });
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Export includes these sections:");
        for (const auto *part : parts(session.state)) {
            ImGui::Text("%s: %zu attacks", partSection(part->settings).c_str(),
                        part->events.size());
        }
        ImGui::Separator();
        ImGui::TextWrapped("Optional: shared patterns, a reference chart for "
                           "comparison, and audio for playback / alignment.");
        pathField("Pattern library", libraryPath, {".sqlite"}, true);
        pathField("Reference", referencePath, {".chart", ".mid", ".gpp"},
                  false);
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
        pathField("Optional audio", audioPath,
                  {".ogg", ".mp3", ".wav", ".flac"}, false);
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
    void alignmentPanel() {
        if (alignmentJob.valid() && alignmentJob.wait_for(std::chrono::seconds(
                                        0)) == std::future_status::ready) {
            perform([&] {
                alignmentResult = alignmentJob.get();
                alignmentCandidate = 0;
                previewAlignment = false;
            });
        }
        help("Aligning MIDI and audio",
             "Audio is optional. Choose the recording in Files. Analyze "
             "compares harmonic features from MIDI and audio, then lists "
             "candidate offsets and tempo scales. It does not apply them "
             "automatically.\n\nOffset shifts the whole chart. Linear also "
             "adjusts the overall tempo. Auto prefers the simplest confident "
             "result. Preview draws candidate positions in white; Apply "
             "changes the project timing and can be undone. The original MIDI "
             "and lane patterns are preserved.\n\nAmbiguous results need "
             "listening and manual confirmation. The silence warning reports "
             "how much silence the recording would need at its start; this "
             "tool does not edit the audio file.");
        ImGui::Text("Applied: t_audio = %.6f * t_MIDI %+.3f s",
                    session.state.alignment.scale,
                    session.state.settings.offset);
        ImGui::Combo("Alignment mode", &alignmentMode,
                     "Offset\0Linear\0Auto\0");
        alignmentSettings.mode = alignmentMode == 0   ? "offset"
                                 : alignmentMode == 1 ? "linear"
                                                      : "auto";
        ImGui::InputDouble("Minimum offset (s)",
                           &alignmentSettings.minimumOffset);
        ImGui::InputDouble("Maximum offset (s)",
                           &alignmentSettings.maximumOffset);
        if (alignmentJob.valid()) {
            ImGui::TextUnformatted("Analyzing audio and score...");
            if (ImGui::Button("Cancel analysis")) {
                cancelAlignment = true;
            }
        } else if (ImGui::Button("Analyze alignment")) {
            perform([&] {
                alignmentSettings.validate();
                if (!system) {
                    gpp::audio::DeviceSettings settings;
                    settings.device = audioDevice;
                    system = std::make_unique<gpp::audio::BassSystem>(settings);
                }
                cancelAlignment = false;
                alignmentResult.reset();
                previewAlignment = false;
                const auto state = session.state;
                const auto path = fs::u8path(audioPath);
                const auto settings = alignmentSettings;
                const auto device = system->device();
                alignmentJob = std::async(
                    std::launch::async, [this, state, path, settings, device] {
                        gpp::audio::check(BASS_SetDevice(DWORD(device)),
                                          "Select alignment decoder device");
                        return analyzeAlignment(state, path, settings, [this] {
                            if (cancelAlignment.load()) {
                                throw std::runtime_error("Alignment cancelled");
                            }
                        });
                    });
            });
        }
        if (alignmentResult) {
            const auto &r = *alignmentResult;
            ImGui::TextWrapped("%s", r.diagnostic.c_str());
            ImGui::Text("Candidate margin: %.3f", r.margin);
            for (size_t i = 0; i < r.candidates.size(); ++i) {
                const auto &c = r.candidates[i];
                const auto label = std::to_string(i) + ": offset " +
                                   std::to_string(c.offset) + " scale " +
                                   std::to_string(c.scale);
                ImGui::RadioButton(label.c_str(), &alignmentCandidate, int(i));
                ImGui::Text("score %.3f, coverage %.1f%%", c.score,
                            c.coverage * 100);
            }
            ImGui::Checkbox("Preview alignment (white outlines)",
                            &previewAlignment);
            if (ImGui::Button("Apply selected alignment")) {
                perform([&] {
                    if (fs::absolute(fs::u8path(audioPath))
                                .lexically_normal() !=
                            fs::u8path(r.audioPath).lexically_normal() ||
                        fileHash(fs::u8path(audioPath)) != r.audioHash) {
                        throw std::runtime_error(
                            "Audio changed; analyze again");
                    }
                    applyAlignment(session, r, size_t(alignmentCandidate));
                    session.state.audio = r.audioPath;
                    previewAlignment = false;
                });
            }
            for (const auto &w : r.windows) {
                ImGui::Text("At %.1fs: residual %+.2fs (best candidate)",
                            w.midiTime, w.residual);
            }
            if (alignmentSilence(r, size_t(alignmentCandidate)) > 0) {
                ImGui::TextWrapped(
                    "Suggested initial audio silence: %.3f s. Add manually and "
                    "increase offset equally; audio is unchanged.",
                    alignmentSilence(r, size_t(alignmentCandidate)));
            }
        }
        if (ImGui::Button("Reset alignment")) {
            perform([&] {
                session.setAlignment(1, 0, "");
                previewAlignment = false;
            });
        }
    }
    void tracks() {
        help("Game parts and musical tracks",
             "Add one row per game instrument/difficulty. Each row maps ONE "
             "musical MIDI track "
             "to its own section in the exported chart. Guitar, bass, rhythm, "
             "co-op and Keys all use five lanes. "
             "Drum/vocal/Pro destinations are not supported.\n\nSelect a row "
             "to edit its source, mapping, Plus "
             "and notes. All parts share audio alignment and playback "
             "position. Generate all parts processes "
             "every row; Export writes all sections into one file.\n\nPrograms "
             "describe MIDI playback sounds, "
             "not necessarily the actual instrument: tablatures often use "
             "sax/clarinet for vocals. "
             "Combining several guitars or bass tracks into one part is "
             "planned, not implemented. "
             "Use channel/program filters only when needed; -1 keeps every "
             "channel/program in the selected track.");
        ImGui::TextWrapped("2. Assign each game part to a musical track. "
                           "Select a row to edit it.");
        uint64_t chosen = 0;
        if (ImGui::BeginTable("Parts", 3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Game part");
            ImGui::TableSetupColumn("MIDI track");
            ImGui::TableSetupColumn("Attacks");
            ImGui::TableHeadersRow();
            for (const auto *part : parts(session.state)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const auto label = partSection(part->settings);
                if (ImGui::Selectable(label.c_str(),
                                      part->partId == session.state.partId,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    chosen = part->partId;
                }
                ImGui::TableNextColumn();
                ImGui::Text("%d", part->settings.track);
                ImGui::TableNextColumn();
                const auto attacks = std::count_if(
                    part->events.begin(), part->events.end(),
                    [](const Event &event) { return !event.removed; });
                ImGui::Text("%zu", static_cast<size_t>(attacks));
            }
            ImGui::EndTable();
        }
        if (chosen) {
            perform([&] {
                session.selectPart(chosen);
                changedPart();
            });
        }
        const char *names[] = {"Lead guitar", "Bass", "Rhythm guitar",
                               "Co-op guitar", "Keys (5 lanes)"};
        const char *roles[] = {"guitar", "bass", "rhythm", "coop", "keys"};
        const char *levels[] = {"expert", "hard", "medium", "easy"};
        ImGui::Combo("New instrument", &addInstrument, names, 5);
        ImGui::Combo("New difficulty", &addDifficulty, levels, 4);
        if (ImGui::Button("Add part")) {
            perform([&] {
                Settings settings;
                settings.instrument = roles[addInstrument];
                settings.difficulty = levels[addDifficulty];
                session.addPart(settings);
                changedPart();
            });
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(session.state.otherParts.empty());
        if (ImGui::Button("Remove active part")) {
            perform([&] {
                session.removePart(session.state.partId);
                changedPart();
            });
        }
        ImGui::EndDisabled();
        hint("Removes this part and its edits. Undo restores it.");
        ImGui::Separator();
        ImGui::Text("Editing %s", partSection(session.state.settings).c_str());
        if (sourceDraftPart != session.state.partId) {
            sourceDraft.reset();
            sourceDraftPart = session.state.partId;
        }
        auto settings = sourceDraft.value_or(session.state.settings);
        auto trackLabel = [&](int index) {
            if (index < 0 ||
                size_t(index) >= session.state.score.tracks.size()) {
                return std::string("Choose a musical track...");
            }
            const auto &track = session.state.score.tracks[size_t(index)];
            std::set<int> programs, channels;
            for (const auto &note : track.notes) {
                programs.insert(note.program);
                channels.insert(note.channel);
            }
            std::string label = std::to_string(index) + ": " +
                                (track.name.empty() ? "Unnamed" : track.name) +
                                " | " + std::to_string(track.notes.size()) +
                                " notes | programs";
            for (int program : programs) {
                label += " " + std::to_string(program);
            }
            label += " | channels";
            for (int channel : channels) {
                label += " " + std::to_string(channel);
            }
            return label;
        };
        bool sourceChanged = false;
        const auto label = trackLabel(settings.track);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##SourceTrack", label.c_str())) {
            for (size_t i = 0; i < session.state.score.tracks.size(); ++i) {
                const auto item = trackLabel(int(i));
                if (ImGui::Selectable(item.c_str(), settings.track == int(i))) {
                    settings.track = int(i);
                    settings.channel = settings.program = -1;
                    sourceChanged = true;
                }
            }
            ImGui::EndCombo();
        }
        hint(label.c_str());
        if (ImGui::TreeNode("Source filters (optional)")) {
            sourceChanged |=
                ImGui::InputInt("Channel (-1: all)", &settings.channel);
            sourceChanged |=
                ImGui::InputInt("Program (-1: all)", &settings.program);
            ImGui::TreePop();
        }
        if (sourceChanged) {
            if (session.state.events.empty() &&
                session.state.plus.phrases.empty()) {
                perform([&] {
                    session.configurePart(settings);
                    sourceDraft.reset();
                });
            } else {
                sourceDraft = settings;
            }
        }
        if (sourceDraft) {
            ImGui::TextWrapped("Source changed. Reset only this part's notes, "
                               "cache and Plus? Undo remains available.");
            if (ImGui::Button("Reset this part to new source")) {
                perform([&] {
                    session.configurePart(*sourceDraft, true);
                    changedPart();
                });
            }
            if (ImGui::Button("Keep current source")) {
                sourceDraft.reset();
            }
        }
        if (ImGui::TreeNode("Track details (names, programs, chords, range)")) {
            const auto details = inspect(session.state.score);
            ImGui::TextWrapped("%s", details.c_str());
            ImGui::TreePop();
        }
        if (hasTrack()) {
            const auto &notes =
                session.state.score.tracks[size_t(session.state.settings.track)]
                    .notes;
            if (std::any_of(notes.begin(), notes.end(), [](const ScoreNote &n) {
                    return n.channel == 9;
                })) {
                ImGui::TextWrapped("Percussion detected: five-lane melodic "
                                   "mapping is not a drum converter. Select a "
                                   "melodic source or filter channel 9 out.");
            }
        }
        batchControls();
    }
    void algorithm() {
        help("Generation and settings",
             "Generate maps real pitches and chords to five lanes. Start with "
             "Expert and Beam search; lower difficulties are experimental "
             "reduction profiles. Keep attacks = 1 retains every attack; "
             "smaller values reduce density.\n\nReuse cache uses established "
             "mappings. Reuse transposed phrases recognizes identical "
             "intervals and rhythm in a different key. Confirmed and locked "
             "choices survive regeneration. Weights affect newly calculated "
             "decisions; they do not erase confirmed patterns.\n\nChannel and "
             "Program filters are zero-based; -1 includes all. They help "
             "isolate instruments in Type 0 MIDI. Mapping settings apply when "
             "you Generate again.\n\nUMAP requires an external projection "
             "file; it is not a self-contained UI workflow yet. Beam search "
             "and Heuristic work entirely in the interface.");
        ImGui::TextWrapped("3. Choose settings, then Generate chart.");
        auto &s = session.state.settings;
        const char *difficulties[] = {"expert", "hard", "medium", "easy"};
        int difficulty = 0;
        for (int i = 0; i < 4; ++i) {
            if (s.difficulty == difficulties[i]) {
                difficulty = i;
            }
        }
        if (ImGui::Combo("Difficulty", &difficulty, difficulties, 4)) {
            perform([&] {
                auto next = s;
                next.difficulty = difficulties[difficulty];
                session.configurePart(next);
            });
        }
        if (difficulty) {
            ImGui::TextWrapped("Experimental reduction profile");
        }
        int strategy = s.algorithm == "beam"        ? 0
                       : s.algorithm == "heuristic" ? 1
                                                    : 2;
        if (ImGui::Combo("Algorithm", &strategy,
                         "Beam search\0Heuristic\0UMAP (experimental)\0")) {
            s.algorithm = strategy == 0   ? "beam"
                          : strategy == 1 ? "heuristic"
                                          : "umap";
            session.dirty = true;
        }
        ImGui::Text("Part: %s (source filters in Tracks)",
                    partSection(s).c_str());
        session.dirty |= ImGui::SliderInt("Max chord lanes", &s.maxChord, 1, 3);
        session.dirty |=
            ImGui::InputDouble("Keep attacks (0..1)", &s.density, .05);
        session.dirty |= ImGui::Checkbox("Reuse cache", &s.reuse);
        session.dirty |=
            ImGui::Checkbox("Reuse transposed phrases", &s.structural);
        hint("Recognize the same intervals and rhythm in another key; the "
             "pitches need not be identical.");
        if (strategy == 2) {
            pathField("UMAP artifact", projectionPath, {".json"}, false);
            if (ImGui::Button("Load projection")) {
                perform([&] {
                    session.projection =
                        readProjection(fs::u8path(projectionPath));
                    s.algorithm = "umap:" + session.projection->identity;
                    session.dirty = true;
                });
            }
            ImGui::TextWrapped("Experimental. Create artifact with "
                               "experiments/umap_projection.py; reload it "
                               "after opening a project.");
        }
        double offset = s.offset * 1000;
        if (ImGui::InputDouble("Audio offset (ms)", &offset, 10)) {
            perform([&] {
                auto settings = s;
                settings.offset = offset / 1000;
                settings.validate();
                session.setAlignment(session.state.alignment.scale,
                                     settings.offset,
                                     session.state.alignment.evidence);
            });
        }
        help("Sustain cutoff",
             "Export can omit very short sustain tails while preserving the "
             "musical durations in the project. In beat mode, a sustain must "
             "be strictly longer than the chosen limit (0.5 = half a beat). In "
             "seconds mode, the limit is an absolute duration. This is an "
             "export policy; it does not change MIDI notes or the pattern "
             "cache.");
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
        ImGui::BeginDisabled(!hasTrack());
        if (ImGui::Button("Generate chart")) {
            perform([&] { generate(); });
        }
        ImGui::EndDisabled();
        if (!hasTrack()) {
            ImGui::TextWrapped(
                "Select a track in Tracks to enable generation.");
        }
        ImGui::Separator();
        help("Regenerating a region",
             "Place the playback cursor in Timeline and mark the region start "
             "and end below. Regenerate only changes this interval, preserving "
             "protected decisions. Restore returns the region to the original "
             "generated result. Tick fields are an optional precise "
             "alternative; one beat equals the MIDI resolution in ticks.");
        if (ImGui::Button("Set start at cursor")) {
            regionBegin = audioTick(session.state, cursor);
        }
        if (ImGui::Button("Set end at cursor")) {
            regionEnd = audioTick(session.state, cursor);
        }
        ImGui::Text("Region: %.2f to %.2f s",
                    audioSeconds(session.state, regionBegin),
                    audioSeconds(session.state, regionEnd));
        if (ImGui::TreeNode("Exact region ticks")) {
            ImGui::InputScalar("Start tick", ImGuiDataType_S64, &regionBegin);
            ImGui::InputScalar("End tick", ImGuiDataType_S64, &regionEnd);
            ImGui::TreePop();
        }
        ImGui::BeginDisabled(!hasTrack() || regionBegin < 0 ||
                             regionEnd <= regionBegin);
        if (ImGui::Button("Regenerate region")) {
            perform([&] {
                session.run(loadLibrary(fs::u8path(libraryPath)),
                            {{regionBegin, regionEnd}});
            });
        }
        if (ImGui::Button("Restore region")) {
            perform([&] { session.restore(regionBegin, regionEnd); });
        }
        ImGui::EndDisabled();
    }

    void editing() {
        help("Editing and remembered patterns",
             "Click a generated note in Timeline. Ctrl-click extends "
             "selection; Copy and Delete use the selection. Lane/time/sustain "
             "fields edit the first selected event. Drag a note to move it; "
             "Shift-drag changes its sustain. Keys 1-5 toggle "
             "lanes.\n\nConfirm remembers the phrase; Lock protects it against "
             "regeneration. Enable Apply to equivalent occurrences to teach "
             "all matching phrases. Otherwise this occurrence can become a "
             "separate variant. Promote shares a confirmed pattern with other "
             "projects via the library file.\n\nUnlock returns a pattern to "
             "experimental status. Recalculate explicitly requests a new "
             "pattern. Clear experimental cache preserves confirmed and locked "
             "decisions. Save project persists your edits.");
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
            perform([&] { session.paste(audioTick(session.state, cursor)); });
        }
        if (ImGui::Button("Insert at cursor")) {
            perform([&] {
                session.insert(audioTick(session.state, cursor), 1, 0);
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete selected")) {
            session.removeSelected();
        }
        ImGui::Checkbox("Apply to equivalent occurrences", &propagate);
        if (session.selection.empty()) {
            ImGui::TextWrapped("Select a generated note in Timeline to edit "
                               "its lanes or teach a phrase.");
        }
        if (const auto *event = selected()) {
            const Event e = *event;
            ImGui::Text("Selected: %zu | editing event %llu",
                        session.selection.size(),
                        static_cast<unsigned long long>(e.note.id));
            int mask = e.note.flags;
            int64_t tick = e.tick, duration = e.duration;
            bool edited = false;
            const char *names[] = {"Green", "Red", "Yellow", "Blue", "Orange"};
            for (int lane = 0; lane < 5; ++lane) {
                bool enabled = mask & (1 << lane);
                ImGui::PushStyleColor(ImGuiCol_Text, colors[lane]);
                if (ImGui::Checkbox(names[lane], &enabled)) {
                    mask ^= 1 << lane;
                    edited = true;
                }
                ImGui::PopStyleColor();
            }
            double position = audioSeconds(session.state, tick);
            double sustain =
                audioSeconds(session.state, tick + duration) - position;
            if (ImGui::InputDouble("Start (s)", &position, .01)) {
                tick = audioTick(session.state, position);
                edited = true;
            }
            if (ImGui::InputDouble("Sustain (s)", &sustain, .01)) {
                duration = std::max<int64_t>(
                    0, audioTick(session.state,
                                 audioSeconds(session.state, tick) + sustain) -
                           tick);
                edited = true;
            }
            if (ImGui::TreeNode("Exact note ticks")) {
                edited |= ImGui::InputScalar("Tick", ImGuiDataType_S64, &tick);
                edited |= ImGui::InputScalar("Sustain ticks", ImGuiDataType_S64,
                                             &duration);
                ImGui::TreePop();
            }
            if (edited) {
                perform([&] {
                    session.edit(e.note.id, mask, tick, duration, propagate);
                });
            }
            ImGui::TextWrapped("Phrase %s\nOrigin: %s\nMatch: %s\nState: %s",
                               e.phrase.c_str(), e.origin.c_str(),
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
    void plusPanel() {
        ImGui::PushItemWidth(
            std::max(80.f, ImGui::GetContentRegionAvail().x * .42f));
        const auto key = std::to_string(session.documentGeneration) +
                         session.state.inputHash + ":" +
                         std::to_string(session.state.partId);
        if (key != plusLoaded) {
            plusDraft = session.state.plus.settings;
            plusLoaded = key;
            plusResult.reset();
            plusEdit = {};
        }
        if (plusJob.valid() && plusJob.wait_for(std::chrono::seconds(0)) ==
                                   std::future_status::ready) {
            perform([&] {
                auto result = plusJob.get();
                if (plusRevision != session.revision ||
                    result.proposal.source != hash(encode(session.state))) {
                    throw std::runtime_error("Document changed; Plus analysis "
                                             "discarded. Analyze again.");
                }
                plusResult = std::move(result);
                notice = "Plus preview ready. Inspect reasons and simulation, "
                         "then Apply proposal.";
            });
        }
        help(
            "Plus: planning and editing",
            "1. Enable Plus and choose a profile. 2. Analyze and propose "
            "creates a preview only. "
            "3. Compare energy and reasons. 4. Apply proposal changes the "
            "project and supports Undo. "
            "Save project keeps phrases; Export writes playable S 2 events.\n\n"
            "Reserve 0.4 means 40%, or two successful phrases and eight "
            "seconds of activation. "
            "This is a preference, not a guarantee. Manual, confirmed and "
            "locked phrases are preserved. "
            "Unlock before moving or deleting. A region uses the Start/End "
            "controls in Mapping.\n\n"
            "Simulation requires every attack in the phrase. Extra strums and "
            "judgment windows are not modeled. Skill is independent success "
            "per "
            "attack, not phrase success. "
            "Risk and score benefit are design approximations. The energy plot "
            "shows trial 0, not a prediction.");
        ImGui::Checkbox("Enable Plus generation", &plusDraft.enabled);
        ImGui::Checkbox("Show Plus timeline overlays", &showPlusOverlay);
        const char *profiles[] = {"balanced", "strategic", "generous",
                                  "sparse"};
        if (ImGui::BeginCombo("Profile", plusDraft.profile.c_str())) {
            for (auto profile : profiles) {
                if (ImGui::Selectable(profile, plusDraft.profile == profile)) {
                    const bool enabled = plusDraft.enabled;
                    plusDraft = plusProfile(profile);
                    plusDraft.enabled = enabled;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputDouble("Reserve (0-1)", &plusDraft.reserve, .05, .2,
                           "%.2f");
        ImGui::InputInt("Phrase measures (1-2)", &plusDraft.preferredBars);
        ImGui::InputDouble("Spacing (measures)", &plusDraft.spacingBars, 1, 4,
                           "%.1f");
        if (ImGui::TreeNode("Planning weights")) {
            ImGui::InputDouble("Musical boundaries",
                               &plusDraft.structureWeight);
            ImGui::InputDouble("Future reserve", &plusDraft.strategyWeight);
            ImGui::InputDouble("Completion risk", &plusDraft.riskWeight);
            ImGui::TreePop();
        }
        if (ImGui::Button("Save Plus settings")) {
            perform([&] { session.configurePlus(plusDraft); });
        }
        ImGui::Checkbox("Only selected tick region", &plusRegionOnly);
        ImGui::Combo("Activation policy", &plusPolicy,
                     "Save for dense regions\0Activate immediately\0Wait for "
                     "reserve\0Approximate future value\0Never activate\0");
        ImGui::Combo("Success per attack", &plusSkill, "100%\090%\075%\050%\0");
        const bool busy = plusJob.valid();
        ImGui::BeginDisabled(busy || session.state.events.empty());
        const auto launch = [&](bool generate) {
            const auto snapshot = session.state;
            const auto parameters = plusDraft;
            parameters.validate();
            std::optional<std::pair<int64_t, int64_t>> region;
            if (plusRegionOnly) {
                if (regionEnd <= regionBegin) {
                    throw std::runtime_error(
                        "Select a valid region in Mapping first");
                }
                region = {{regionBegin, regionEnd}};
            }
            PlusSimulationSettings simulation;
            const char *policies[] = {"dense", "immediate", "reserve", "score",
                                      "never"};
            const double skills[] = {1., .9, .75, .5};
            simulation.strategy = policies[plusPolicy];
            simulation.success = skills[plusSkill];
            simulation.reserve = std::max(.01, parameters.reserve);
            cancelPlus = false;
            plusRevision = session.revision;
            plusJob = std::async(
                std::launch::async, [this, snapshot, parameters, region,
                                     simulation, generate]() mutable {
                    const auto check = [this] {
                        if (cancelPlus.load()) {
                            throw std::runtime_error("Plus analysis cancelled");
                        }
                    };
                    PlusJobResult result;
                    if (generate) {
                        result.proposal =
                            proposePlus(snapshot, parameters, region, check);
                    } else {
                        result.proposal.source = hash(encode(snapshot));
                        result.proposal.document = snapshot.plus;
                        result.proposal.analysis = analyzePlus(snapshot);
                        result.proposal.diagnostics = plusProblems(snapshot);
                    }
                    result.sequence = simulatePlus(
                        result.proposal.analysis,
                        result.proposal.document.phrases, simulation, check);
                    return result;
                });
        };
        ImGui::BeginDisabled(!plusDraft.enabled);
        if (ImGui::Button("Analyze and propose Plus")) {
            perform([&] { launch(true); });
        }
        ImGui::EndDisabled();
        if (ImGui::Button("Simulate applied phrases")) {
            perform([&] { launch(false); });
        }
        ImGui::EndDisabled();
        if (busy) {
            ImGui::TextUnformatted("Analyzing immutable snapshot...");
            if (ImGui::Button("Cancel Plus analysis")) {
                cancelPlus = true;
            }
        }
        if (plusResult && plusRevision != session.revision) {
            plusResult.reset();
        }
        if (plusResult) {
            const auto &r = *plusResult;
            ImGui::Text("Preview: %zu phrases, %zu target regions",
                        r.proposal.document.phrases.size(),
                        r.proposal.analysis.regions.size());
            ImGui::TextWrapped(
                "Simulation: completed %.2f; activation %.1f s; wasted energy "
                "%.2f. Requires every attack in each phrase.",
                r.sequence.completed, r.sequence.activeSeconds,
                r.sequence.wasted);
            if (ImGui::Button("Apply Plus proposal")) {
                perform([&] {
                    session.applyPlus(plusResult->proposal);
                    plusResult.reset();
                    notice = "Plus applied. Save project or Export .chart; "
                             "Undo restores previous phrases.";
                });
            }
            if (plusResult) {
                for (const auto &diagnostic :
                     plusResult->proposal.diagnostics) {
                    ImGui::TextWrapped("%s", diagnostic.c_str());
                }
                if (ImGui::TreeNode("Preview reasons")) {
                    for (const auto &phrase :
                         plusResult->proposal.document.phrases) {
                        ImGui::TextWrapped(
                            "#%llu risk %.2f: %s",
                            static_cast<unsigned long long>(phrase.id),
                            phrase.risk, phrase.reason.c_str());
                    }
                    ImGui::TreePop();
                }
            }
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Applied phrases: select to edit");
        ImGui::BeginChild("Plus phrase list", {0, 140}, true);
        for (const auto &phrase : session.state.plus.phrases) {
            const auto label = "Plus #" + std::to_string(phrase.id) + " [" +
                               std::to_string(phrase.begin) + ", " +
                               std::to_string(phrase.end) + ")";
            if (ImGui::Selectable(label.c_str(), plusEdit.id == phrase.id)) {
                plusEdit = phrase;
                seek(audioSeconds(session.state, phrase.begin));
            }
        }
        ImGui::EndChild();
        ImGui::InputScalar("Plus start tick", ImGuiDataType_S64,
                           &plusEdit.begin);
        ImGui::InputScalar("Plus end tick (exclusive)", ImGuiDataType_S64,
                           &plusEdit.end);
        ImGui::Combo("Protection", &plusEdit.status,
                     "Editable\0Confirmed\0Locked\0");
        ImGui::BeginDisabled(plusEdit.id == 0);
        if (ImGui::Button("Save phrase edit")) {
            perform([&] { session.editPlus(plusEdit); });
        }
        ImGui::SameLine();
        ImGui::EndDisabled();
        if (ImGui::Button("New phrase from region")) {
            perform([&] {
                PlusPhrase phrase;
                phrase.begin = regionBegin;
                phrase.end = regionEnd;
                session.editPlus(phrase);
            });
        }
        if (plusEdit.id && ImGui::Button("Delete selected Plus")) {
            perform([&] {
                session.editPlus(plusEdit, true);
                plusEdit = {};
            });
        }
        if (plusEdit.id && ImGui::TreeNode("Selected phrase details")) {
            ImGui::TextWrapped("Generation rationale: %s",
                               plusEdit.reason.c_str());
            ImGui::Text("Generation risk: %.2f (heuristic)", plusEdit.risk);
            ImGui::TreePop();
        }
        ImGui::PopItemWidth();
    }

    void canvas() {
        ImGui::BeginChild(
            "timeline",
            {0, std::max(120.f, ImGui::GetContentRegionAvail().y - 180)}, true);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 size{std::max(1.f, available.x),
                          std::max(1.f, available.y)};
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
        const double end = start + size.x / scale;
        if (!score.tempos.empty()) {
            int64_t beginTick = audioTick(session.state, start);
            const int64_t endTick = audioTick(session.state, end);
            beginTick = beginTick / score.ppq * score.ppq;
            for (int64_t tick = beginTick;
                 tick <= endTick &&
                 tick - beginTick < int64_t(score.ppq) * 1000;
                 tick += score.ppq) {
                const float xx = x(audioSeconds(session.state, tick));
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
                const float xx = x(audioSeconds(session.state, tempo.tick));
                if (xx >= origin.x && xx <= origin.x + size.x) {
                    draw->AddText(
                        {xx + 3, origin.y + piano - 16},
                        IM_COL32(235, 195, 95, 255),
                        (std::to_string(int(60000000.0 /
                                            (tempo.microseconds *
                                             session.state.alignment.scale))) +
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
                const double at = audioSeconds(session.state, note.start),
                             finish = audioSeconds(session.state, note.end);
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
        if (previewAlignment && alignmentResult &&
            alignmentResult->midiHash == session.state.inputHash) {
            const auto &candidate =
                alignmentResult->candidates.at(size_t(alignmentCandidate));
            for (const auto &event : session.state.events) {
                const double time =
                    candidate.scale * score.seconds(event.tick) +
                    candidate.offset;
                if (event.removed || time < start || time > end) {
                    continue;
                }
                for (int lane = 0; lane < 5; ++lane) {
                    if (event.note.flags & (1 << lane)) {
                        const float yy =
                            origin.y + laneTop + float(lane) * laneHeight;
                        draw->AddRect({x(time) - 5, yy - 9},
                                      {x(time) + 5, yy + 9}, IM_COL32_WHITE);
                    }
                }
            }
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
        if (showPlusOverlay) {
            const bool preview = plusResult && plusRevision == session.revision;
            const auto &phrases = preview
                                      ? plusResult->proposal.document.phrases
                                      : session.state.plus.phrases;
            for (const auto &phrase : phrases) {
                const float left = x(audioSeconds(session.state, phrase.begin)),
                            right = x(audioSeconds(session.state, phrase.end));
                draw->AddRectFilled(
                    {left, origin.y + laneTop - 12},
                    {right, origin.y + laneTop + 5 * laneHeight},
                    preview ? IM_COL32(190, 120, 255, 45)
                            : IM_COL32(80, 210, 255, 45));
                draw->AddText({left, origin.y + laneTop - 12},
                              IM_COL32(210, 235, 255, 255), "Plus");
            }
            if (preview) {
                for (const auto &r : plusResult->proposal.analysis.regions) {
                    draw->AddRectFilled({x(r.time), origin.y + size.y - 16},
                                        {x(r.endTime), origin.y + size.y - 5},
                                        IM_COL32(255, 160, 50, 180));
                }
                // One-second attack density, normalized to the visible maximum.
                std::map<int, int> density;
                int peak = 1;
                for (const auto &attack :
                     plusResult->proposal.analysis.attacks) {
                    if (attack.time >= start && attack.time <= end) {
                        peak = std::max(
                            peak, ++density[int(std::floor(attack.time))]);
                    }
                }
                for (const auto &bin : density) {
                    draw->AddRectFilled(
                        {x(bin.first),
                         origin.y + size.y - 80 -
                             float(bin.second) * 20 / float(peak)},
                        {x(bin.first + 1), origin.y + size.y - 80},
                        IM_COL32(210, 210, 220, 110));
                }
                const auto &trace = plusResult->sequence.trajectory;
                for (size_t i = 1; i < trace.size(); ++i) {
                    draw->AddLine(
                        {x(trace[i - 1].time),
                         origin.y + size.y - 20 -
                             float(trace[i - 1].energy) * 40},
                        {x(trace[i].time),
                         origin.y + size.y - 20 - float(trace[i].energy) * 40},
                        IM_COL32(80, 230, 255, 255), 2);
                }
                for (const auto &activation :
                     plusResult->sequence.activations) {
                    draw->AddLine({x(activation.begin), origin.y + size.y - 70},
                                  {x(activation.end), origin.y + size.y - 70},
                                  IM_COL32(100, 255, 120, 255), 4);
                }
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
                    const auto tick = audioTick(
                        session.state,
                        audioSeconds(session.state, dragEvent.tick) + delta);
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
        ImGui::Checkbox("Loop region", &loopRegion);
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
        ImGui::BeginDisabled(reference.notes.empty() ||
                             session.state.events.empty());
        if (ImGui::Button("Export comparison.json")) {
            perform([&] {
                safeWrite(fs::u8path(outputPath).parent_path() /
                              "comparison.json",
                          compare(asReference(session.state), reference, 0,
                                  referenceOffset),
                          replaceOutput);
            });
        }
        ImGui::EndDisabled();
        if (ImGui::TreeNode("Generation diagnostics")) {
            ImGui::TextWrapped("%s", report(session.state).c_str());
            ImGui::TreePop();
        }
    }
    bool frame(GLFWwindow *window) {
        if (generationJob.valid() &&
            generationJob.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready) {
            perform([&] {
                auto result = generationJob.get();
                if (cancelGeneration.load()) {
                    throw std::runtime_error(
                        "Batch cancelled; previous parts preserved");
                }
                session.applyGenerated(std::move(result), generationSource);
                plusLoaded.clear();
                notice = "All parts generated. Review each row, Save project, "
                         "then Export the combined .chart.";
            });
        }
        tick();
        auto &io = ImGui::GetIO();
        if (!io.WantTextInput) {
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
                perform([&] { saveProject(); });
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
                session.undo();
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
                session.redo();
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G)) {
                perform([&] { generate(); });
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
                perform(
                    [&] { session.paste(audioTick(session.state, cursor)); });
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
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Panels")) {
                ImGui::MenuItem("Files", nullptr, &showFiles);
                ImGui::MenuItem("Tracks", nullptr, &showTracks);
                ImGui::MenuItem("Mapping", nullptr, &showMapping);
                ImGui::MenuItem("Edit and patterns", nullptr, &showEditing);
                ImGui::MenuItem("Audio alignment", nullptr, &showAlignment);
                ImGui::MenuItem("Plus", nullptr, &showPlus);
                if (ImGui::MenuItem("Reset layout")) {
                    resetLayout = true;
                    showFiles = showTracks = showMapping = showEditing =
                        showAlignment = showPlus = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Help / workflow")) {
                helpTitle = "Getting started";
                helpText = workflowHelp;
                showHelp = true;
            }
            ImGui::TextUnformatted(session.dirty ? "Unsaved project changes"
                                                 : "Project unchanged");
            ImGui::EndMainMenuBar();
        }
        const auto dock =
            ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        if (resetLayout || (firstLayout && !hasSavedLayout)) {
            ImGui::DockBuilderRemoveNode(dock);
            ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dock,
                                          ImGui::GetMainViewport()->WorkSize);
            ImGuiID center = dock;
            const auto left = ImGui::DockBuilderSplitNode(
                center, ImGuiDir_Left, .27f, nullptr, &center);
            const auto right = ImGui::DockBuilderSplitNode(
                center, ImGuiDir_Right, .36f, nullptr, &center);
            ImGui::DockBuilderDockWindow("Files", left);
            ImGui::DockBuilderDockWindow("Tracks", left);
            ImGui::DockBuilderDockWindow("Mapping", right);
            ImGui::DockBuilderDockWindow("Edit and patterns", right);
            ImGui::DockBuilderDockWindow("Audio alignment", right);
            ImGui::DockBuilderDockWindow("Plus", right);
            ImGui::DockBuilderDockWindow("Timeline", center);
            ImGui::DockBuilderFinish(dock);
        }
        firstLayout = resetLayout = false;
        if (showFiles) {
            if (ImGui::Begin("Files", &showFiles)) {
                ImGui::PushItemWidth(
                    std::max(80.f, ImGui::GetContentRegionAvail().x * .38f));
                files();
                ImGui::PopItemWidth();
            }
            ImGui::End();
        }
        if (showTracks) {
            if (ImGui::Begin("Tracks", &showTracks)) {
                ImGui::PushItemWidth(
                    std::max(80.f, ImGui::GetContentRegionAvail().x * .38f));
                tracks();
                ImGui::PopItemWidth();
            }
            ImGui::End();
        }
        if (showMapping) {
            if (ImGui::Begin("Mapping", &showMapping)) {
                ImGui::PushItemWidth(
                    std::max(80.f, ImGui::GetContentRegionAvail().x * .38f));
                algorithm();
                ImGui::PopItemWidth();
            }
            ImGui::End();
        }
        if (showEditing) {
            if (ImGui::Begin("Edit and patterns", &showEditing)) {
                ImGui::PushItemWidth(
                    std::max(80.f, ImGui::GetContentRegionAvail().x * .38f));
                editing();
                ImGui::PopItemWidth();
            }
            ImGui::End();
        }
        if (showAlignment) {
            if (ImGui::Begin("Audio alignment", &showAlignment)) {
                ImGui::PushItemWidth(
                    std::max(80.f, ImGui::GetContentRegionAvail().x * .38f));
                alignmentPanel();
                ImGui::PopItemWidth();
            }
            ImGui::End();
        }
        if (showPlus) {
            if (ImGui::Begin("Plus", &showPlus)) {
                plusPanel();
            }
            ImGui::End();
        }
        ImGui::Begin("Timeline");
        ImGui::TextWrapped(
            "Open MIDI > Assign parts > Generate > Review > Save / Export");
        if (session.state.score.tracks.empty()) {
            ImGui::TextWrapped("Start in Files: Browse for a musical MIDI, "
                               "then Open MIDI / project. Audio is optional.");
        } else if (!hasTrack()) {
            ImGui::TextWrapped(
                "MIDI loaded. Next: choose the guitar instrument in Tracks.");
            if (ImGui::Button("Choose track")) {
                showTracks = true;
                ImGui::SetWindowFocus("Tracks");
            }
        } else {
            ImGui::TextWrapped(
                "%s | track %d: %s | %zu attacks",
                partSection(session.state.settings).c_str(),
                session.state.settings.track,
                session.state.score.tracks[session.state.settings.track]
                        .name.empty()
                    ? "Unnamed track"
                    : session.state.score.tracks[session.state.settings.track]
                          .name.c_str(),
                session.state.events.size());
            if (ImGui::Button(session.state.events.empty()
                                  ? "Generate chart"
                                  : "Regenerate chart")) {
                perform([&] { generate(); });
            }
        }
        if (!session.state.otherParts.empty()) {
            if (ImGui::BeginCombo(
                    "Active part",
                    partSection(session.state.settings).c_str())) {
                uint64_t chosen = 0;
                for (const auto *part : parts(session.state)) {
                    if (ImGui::Selectable(partSection(part->settings).c_str(),
                                          part->partId ==
                                              session.state.partId)) {
                        chosen = part->partId;
                    }
                }
                if (chosen) {
                    perform([&] {
                        session.selectPart(chosen);
                        changedPart();
                    });
                }
                ImGui::EndCombo();
            }
            batchControls();
        }
        if (!message.empty()) {
            ImGui::TextColored({1, .45f, .35f, 1}, "Action failed:");
            ImGui::TextWrapped("%s", message.c_str());
        } else if (!notice.empty()) {
            ImGui::TextWrapped("%s", notice.c_str());
        }
        if (!session.state.conflicts.empty()) {
            ImGui::TextWrapped("Conflict: %s",
                               session.state.conflicts.front().c_str());
        }
        canvas();
        ImGui::End();
        // A stable ID scope for dialogs, regardless of the originating dock
        // panel.
        ImGui::SetNextWindowPos({-100, -100});
        ImGui::SetNextWindowSize({1, 1});
        ImGui::Begin(
            "Dialog host", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoInputs);
        browser.draw();
        if (showHelp) {
            ImGui::OpenPopup("Chart Studio manual");
            showHelp = false;
        }
        ImGui::SetNextWindowSize({620, 400}, ImGuiCond_FirstUseEver);
        if (ImGui::BeginPopupModal("Chart Studio manual", nullptr)) {
            ImGui::TextUnformatted(helpTitle);
            ImGui::Separator();
            ImGui::BeginChild("manual text", {0, -40});
            ImGui::TextWrapped("%s", helpText);
            ImGui::EndChild();
            if (ImGui::Button("Close manual")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (pendingPrompt) {
            ImGui::OpenPopup("Unsaved project");
            pendingPrompt = false;
        }
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
            if (!message.empty()) {
                ImGui::TextWrapped("%s", message.c_str());
            }
            if (ImGui::Button("Save and close")) {
                perform([&] {
                    saveProject();
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
        if (ImGui::BeginPopupModal("Unsaved project", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Changing document or track discards the "
                                   "current edits. Continue?");
            if (!message.empty()) {
                ImGui::TextWrapped("%s", message.c_str());
            }
            if (ImGui::Button("Save and continue")) {
                perform([&] {
                    saveProject();
                    if (pending) {
                        pending();
                        pending = {};
                    }
                    ImGui::CloseCurrentPopup();
                });
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard and continue")) {
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
        ini = (output / "layout-docking.ini").u8string();
        ImGui::GetIO().IniFilename = ini.c_str();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
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
