#include "synth_ui.h"
#include <imgui.h>
#include <imgui_stdlib.h>
namespace ChartStudio {
SynthPanel::~SynthPanel() {
    cancel = true;
    if (job.valid()) {
        // Shutdown deliberately ignores the cancellation result, but waits
        // before the worker's atomics and other captured members are destroyed.
        job.wait();
    }
}
void SynthPanel::draw(Session &session) {
    if (job.valid() &&
        job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            job.get();
            message = "Stems exported. Original song audio preserved.";
        } catch (const std::exception &e) {
            message = e.what();
        }
    }
    ImGui::SetNextWindowSize(ImVec2(660, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("MIDI Synth")) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Optional offline audio from the original MIDI. Chart "
                       "export never renders audio automatically.");
    if (!synthAvailable()) {
        ImGui::TextWrapped(
            "Unavailable in this build. Install FluidSynth 2 development files "
            "and rebuild with GPP_ENABLE_MIDI_SYNTH=ON. Chart editing remains "
            "available.");
        ImGui::End();
        return;
    }
    auto &c = session.state.synth;
    bool changed = false;
    ImGui::Text("Alignment: audio = MIDI * %.6f %+.3f s",
                session.state.alignment.scale, session.state.settings.offset);
    ImGui::BeginDisabled(job.valid());
    try {
        if (ImGui::Button("Use chart instruments")) {
            c.stems = synthSelection(session.state, "chart");
            c.mode = "chart";
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Use all MIDI sources")) {
            c.stems = synthSelection(session.state, "all");
            c.mode = "all";
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Add stem")) {
            c.mode = "custom";
            c.stems.push_back(
                {"stem" + std::to_string(c.stems.size()), true, {{0, -1, -1}}});
            changed = true;
        }
    } catch (const std::exception &e) {
        message = e.what();
    }
    ImGui::TextWrapped(
        "Empty selection uses chart instruments. Uncheck to exclude; add "
        "sources to combine tracks in one stem. Channels/programs: -1 means "
        "all. Merged parts include their full original MIDI sources; chart "
        "cuts are not audio edits.");
    if (ImGui::TreeNode("MIDI tracks (zero-based)")) {
        for (size_t t = 0; t < session.state.score.tracks.size(); ++t) {
            const auto &track = session.state.score.tracks[t];
            ImGui::Text("%zu: %s (%zu notes)", t, track.name.c_str(),
                        track.notes.size());
        }
        ImGui::TreePop();
    }
    for (size_t i = 0; i < c.stems.size(); ++i) {
        auto &stem = c.stems[i];
        ImGui::PushID(int(i));
        changed |= ImGui::Checkbox("Enabled", &stem.enabled);
        ImGui::SameLine();
        changed |= ImGui::InputText("Filename", &stem.name);
        for (size_t j = 0; j < stem.sources.size(); ++j) {
            auto &source = stem.sources[j];
            ImGui::PushID(int(j));
            changed |= ImGui::InputInt("Track", &source.track);
            changed |= ImGui::InputInt("Channel", &source.channel);
            changed |= ImGui::InputInt("Program", &source.program);
            if (ImGui::Button("Remove source")) {
                stem.sources.erase(stem.sources.begin() + std::ptrdiff_t(j));
                changed = true;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        if (ImGui::Button("Add source")) {
            stem.sources.push_back({0, -1, -1});
            changed = true;
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    changed |= ImGui::InputText("SoundFont .sf2 (path)", &c.soundfont);
    changed |= ImGui::InputText("Output directory", &c.output);
    int format = c.format == "ogg" ? 1 : 0;
    if (ImGui::Combo(
            "Format", &format,
            "WAV (no encoder needed)\0Ogg Vorbis (requires FFmpeg)\0")) {
        c.format = format ? "ogg" : "wav";
        changed = true;
    }
    changed |= ImGui::InputInt("Sample rate", &c.sampleRate);
    changed |= ImGui::SliderInt("Vorbis quality", &c.quality, 0, 10);
    changed |= ImGui::InputDouble("Gain", &c.gain);
    changed |= ImGui::InputDouble("Effect tail (seconds)", &c.tail);
    ImGui::TextWrapped("Save project to retain these settings. Existing "
                       "outputs and song.wav/song.ogg are protected; choose a "
                       "new directory or filename for another render.");
    if (ImGui::Button("Render selected stems")) {
        cancel = false;
        progress = 0;
        message = "Rendering...";
        auto snapshot = session.state;
        auto config = c;
        job = std::async(
            std::launch::async, [this, snapshot = std::move(snapshot), config] {
                renderStems(
                    snapshot, config,
                    [this] {
                        if (cancel) {
                            throw std::runtime_error(
                                "Rendering cancelled; no new stems published");
                        }
                    },
                    [this](double p) { progress = float(p); });
            });
    }
    ImGui::EndDisabled();
    if (changed) {
        session.dirty = true;
    }
    if (job.valid()) {
        ImGui::ProgressBar(progress.load());
        if (ImGui::Button("Cancel render")) {
            cancel = true;
        }
    }
    ImGui::TextWrapped("%s", message.c_str());
    ImGui::End();
}
} // namespace ChartStudio
