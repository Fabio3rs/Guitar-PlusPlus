#include "sfx_designer.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <imgui.h>

namespace AudioStudio {
namespace {
namespace sfx = audio::sfx;

bool text(const char *label, std::string &value) {
    std::array<char, 4097> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
    if (ImGui::InputText(label, buffer.data(), buffer.size())) {
        value = buffer.data();
        return true;
    }

    return false;
}

bool numeric(const char *label, double &value, double low, double high) {
    const bool changed = ImGui::InputDouble(label, &value, 0, 0, "%.5g");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Range %.5g .. %.5g. Applied on Generate.", low,
                          high);
    }

    return changed;
}

template <class T> bool choice(const char *label, T &value, int count) {
    bool changed = false;
    if (ImGui::BeginCombo(label, sfx::name(value))) {
        for (int i = 0; i < count; ++i) {
            const auto next = static_cast<T>(i);
            if (ImGui::Selectable(sfx::name(next), next == value)) {
                value = next;
                changed = true;
            }
        }

        ImGui::EndCombo();
    }

    return changed;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return value;
}
} // namespace

SfxDesigner::SfxDesigner(Session &session) : s(session) {
    auto specs = fs::path(GPP_AUDIO_WORKSPACE) / "data/sounds/specs";
    if (!fs::is_directory(specs) && fs::is_directory("data/sounds/specs")) {
        specs = "data/sounds/specs";
    }
    std::snprintf(root, sizeof(root), "%s", specs.string().c_str());
    if (!s.sfxPath.empty()) {
        std::snprintf(openPath, sizeof(openPath), "%s",
                      s.sfxPath.string().c_str());
        std::snprintf(savePath, sizeof(savePath), "%s",
                      s.sfxPath.string().c_str());
        visible = focus = true;
    }

    attempt([&] { scan(); });
}

void SfxDesigner::open() { visible = focus = true; }

void SfxDesigner::openRecipe(const fs::path &path) {
    open();
    request(Action::Load, path);
}

void SfxDesigner::request(Action action, const fs::path &path) {
    if (s.sfxDirty) {
        pending = action;
        pendingPath = path;
        const auto target =
            s.sfxPath.empty() ? fs::u8path(savePath) : s.sfxPath;
        std::snprintf(confirmSavePath, sizeof(confirmSavePath), "%s",
                      target.string().c_str());
        askToSave = true;
    } else {
        attempt([&] { perform(action, path); });
    }
}

void SfxDesigner::perform(Action action, const fs::path &path) {
    if (action == Action::Load) {
        s.loadSfx(path, true);
        std::snprintf(openPath, sizeof(openPath), "%s", path.string().c_str());
        std::snprintf(savePath, sizeof(savePath), "%s", path.string().c_str());
    } else if (action == Action::New) {
        s.newSfx(true);
        std::snprintf(savePath, sizeof(savePath), "specs/new_sfx.lua");
    }
}

void SfxDesigner::confirmChanges() {
    if (askToSave) {
        ImGui::OpenPopup("Unsaved SFX changes");
        askToSave = false;
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, {.5f, .5f});
    ImGui::SetNextWindowSize({560, 0}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Unsaved SFX changes", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Save your recipe before replacing this draft?");
        ImGui::InputText("Save path", confirmSavePath, sizeof(confirmSavePath));
        if (ImGui::Button("Save and continue")) {
            attempt([&] {
                s.saveSfx(fs::u8path(confirmSavePath));
                perform(pending, pendingPath);
                pending = Action::None;
                ImGui::CloseCurrentPopup();
            });
        }

        ImGui::SameLine();
        if (ImGui::Button("Discard")) {
            attempt([&] {
                perform(pending, pendingPath);
                pending = Action::None;
                ImGui::CloseCurrentPopup();
            });
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            pending = Action::None;
            ImGui::CloseCurrentPopup();
        }

        if (!error.empty()) {
            ImGui::TextWrapped("%s", error.c_str());
        }

        ImGui::EndPopup();
    }
}

void SfxDesigner::scan() { files = sfx::recipes(fs::u8path(root)); }

void SfxDesigner::browser() {
    ImGui::InputText("Recipe folder", root, sizeof(root));
    if (ImGui::Button("Refresh recipes")) {
        attempt([&] { scan(); });
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%zu recipes", files.size());
    ImGui::InputTextWithHint("Filter", "Name / category", filter,
                             sizeof(filter));
    const auto search = lower(filter);
    ImGui::BeginChild("recipe-list", {0, 180}, ImGuiChildFlags_Borders);
    for (const auto &file : files) {
        const auto label =
            file.lexically_relative(fs::u8path(root)).generic_string();
        if (lower(label).find(search) == std::string::npos) {
            continue;
        }

        if (ImGui::Selectable(label.c_str(), file == s.sfxPath)) {
            request(Action::Load, file);
        }
    }

    ImGui::EndChild();
    ImGui::InputText("Open path", openPath, sizeof(openPath));
    if (ImGui::Button("Open recipe")) {
        request(Action::Load, fs::u8path(openPath));
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(s.sfxPath.empty());
    if (ImGui::Button("Reload disk")) {
        request(Action::Load, s.sfxPath);
    }

    ImGui::EndDisabled();

    ImGui::Separator();
    if (ImGui::Button("New from template")) {
        request(Action::New);
    }

    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) {
        s.cancel();
        s.sfxDefinition.id = s.sfxDefinition.id.substr(0, 59) + "_copy";
        s.sfxPath.clear();
        s.editSfx();
        std::snprintf(savePath, sizeof(savePath), "specs/%s.lua",
                      s.sfxDefinition.id.c_str());
    }

    ImGui::InputText("Save as path", savePath, sizeof(savePath));
    if (ImGui::Button("Save Lua")) {
        attempt([&] {
            s.saveSfx(s.sfxPath.empty() ? fs::u8path(savePath) : s.sfxPath);
        });
    }

    ImGui::SameLine();
    if (ImGui::Button("Save As")) {
        attempt([&] { s.saveSfx(fs::u8path(savePath)); });
    }

    ImGui::TextWrapped(
        "Save Lua updates the opened recipe. Save As refuses other existing "
        "files. Relative paths use the Studio output folder. Comments are "
        "replaced by canonical Lua.");
    ImGui::TextWrapped("Opened: %s", s.sfxPath.empty()
                                         ? "Unsaved template"
                                         : s.sfxPath.string().c_str());
}

void SfxDesigner::sound() {
    auto &d = s.sfxDefinition;
    bool changed = text("ID", d.id);
    changed |= text("Category", d.category);
    changed |= text("Description", d.description);
    changed |= numeric("Duration (s)", d.duration, .001, 30);
    int rate = int(d.sampleRate), channels = int(d.channels);
    if (ImGui::InputInt("Sample rate (Hz)", &rate)) {
        d.sampleRate = unsigned(std::max(0, rate));
        changed = true;
    }

    if (ImGui::SliderInt("Channels", &channels, 1, 2)) {
        d.channels = unsigned(channels);
        changed = true;
    }

    changed |= ImGui::InputScalar("Seed", ImGuiDataType_U32, &d.seed);
    changed |= numeric("Output gain (dB)", d.outputGainDb, -80, 12);
    changed |= numeric("Peak ceiling", d.peakLimit, .01, .99);
    ImGui::TextWrapped("Peak protection only attenuates the mix when needed. "
                       "Relative layer levels are preserved.");

    ImGui::SeparatorText("Ambient loop");
    changed |= ImGui::Checkbox("Store loop region", &d.loop.enabled);
    if (d.loop.enabled) {
        changed |= numeric("Begin (s)", d.loop.begin, 0, d.duration);
        changed |= numeric("End (s; 0 = duration)", d.loop.end, 0, d.duration);
        changed |= numeric("Crossfade (s)", d.loop.crossfade, 0, 1);
        ImGui::TextWrapped("Loop bounds use [begin, end). For continuous beds "
                           "use sustain 1 and attack/decay/release 0. "
                           "Crossfade preserves the sound duration.");
    }

    if (changed) {
        s.editSfx();
    }
}

void SfxDesigner::layers() {
    auto &list = s.sfxDefinition.layers;
    bool changed = false;
    ImGui::BeginDisabled(list.size() >= sfx::maxLayers);
    if (ImGui::Button("Add layer")) {
        list.emplace_back();
        selectedLayer = list.size() - 1;
        changed = true;
    }

    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(list.size() <= 1);
    if (ImGui::Button("Remove layer")) {
        selectedLayer = std::min(selectedLayer, list.size() - 1);
        list.erase(list.begin() + selectedLayer);
        changed = true;
    }

    ImGui::EndDisabled();

    selectedLayer = std::min(selectedLayer, list.size() - 1);
    ImGui::BeginChild("layer-list", {0, 115}, ImGuiChildFlags_Borders);
    for (size_t i = 0; i < list.size(); ++i) {
        const auto label = std::to_string(i + 1) + ". " +
                           sfx::name(list[i].type) +
                           (list[i].enabled ? "" : " (bypassed)");
        if (ImGui::Selectable(label.c_str(), selectedLayer == i)) {
            selectedLayer = i;
        }
    }

    ImGui::EndChild();

    auto &l = list[selectedLayer];
    ImGui::PushID(int(selectedLayer));
    changed |= ImGui::Checkbox("Enabled", &l.enabled);
    changed |= choice("Generator", l.type, 3);
    changed |= numeric("Start (s)", l.start, 0, s.sfxDefinition.duration);
    changed |= numeric("Length (s; 0 = remainder)", l.duration, 0,
                       s.sfxDefinition.duration);
    changed |= numeric("Gain", l.gain, 0, 4);
    changed |= numeric("Pan", l.pan, -1, 1);

    if (l.type == sfx::Generator::Noise) {
        changed |= choice("Noise color", l.color, 3);
    } else {
        changed |= numeric("Start frequency (Hz)", l.frequency.startHz, 20,
                           s.sfxDefinition.sampleRate * .45);
        changed |= numeric("End frequency (Hz)", l.frequency.endHz, 20,
                           s.sfxDefinition.sampleRate * .45);
        changed |= choice("Sweep curve", l.frequency.curve, 2);
        if (l.type == sfx::Generator::Pluck) {
            changed |= numeric("String damping", l.damping, .8, .9999);
        } else {
            changed |= choice("Waveform", l.waveform, 5);
            changed |= numeric("Phase (cycles)", l.phase, 0, 1);
            if (l.waveform == sfx::Wave::Pulse) {
                changed |= numeric("Duty cycle", l.duty, .05, .95);
            }
        }
    }

    if (ImGui::CollapsingHeader("Envelope", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= numeric("Attack (s)", l.envelope.attack, 0, 30);
        changed |= numeric("Decay (s)", l.envelope.decay, 0, 30);
        changed |= numeric("Sustain level", l.envelope.sustain, 0, 1);
        changed |= numeric("Release (s)", l.envelope.release, 0, 30);
        changed |=
            ImGui::Checkbox("Exponential envelope", &l.envelope.exponential);
    }

    if (ImGui::CollapsingHeader("Filter / resonance")) {
        changed |= choice("Filter", l.filter.type, 4);
        changed |= numeric("Cutoff (Hz)", l.filter.cutoffHz, 10,
                           s.sfxDefinition.sampleRate * .45);
        changed |= numeric("Q", l.filter.q, .1, 10);
    }

    if (ImGui::CollapsingHeader("Slow modulation")) {
        changed |= numeric("Rate (Hz)", l.modulation.rateHz, 0, 20);
        changed |=
            numeric("Pitch depth (cents)", l.modulation.pitchCents, 0, 1200);
        changed |= numeric("Amplitude depth", l.modulation.amplitude, 0, 1);
    }

    ImGui::PopID();

    if (changed) {
        s.editSfx();
    }
}

void SfxDesigner::result() {
    if (!s.sfxProduct) {
        ImGui::TextWrapped("Open or create a recipe, then Generate. No song or "
                           "FFT analysis is needed.");
        return;
    }

    const auto &p = *s.sfxProduct;
    const auto &wave = p.waveform;
    ImGui::Text("Generated: %s%s", p.definition.id.c_str(),
                s.sfxStale ? " (previous settings)" : "");
    ImGui::Text("%.3f s | %u Hz | %u ch | %zu samples", p.pcm.seconds(),
                p.pcm.rate, p.pcm.channels, p.pcm.samples.size());
    ImGui::Text("Peak %.4f | RMS %.4f | %.2f ms", p.levels.peak, p.levels.rms,
                p.milliseconds);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size{std::max(1.f, ImGui::GetContentRegionAvail().x), 90};
    ImGui::InvisibleButton("SFX waveform", size);
    auto *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y},
                        IM_COL32(14, 19, 26, 255), 4);
    const float mid = origin.y + size.y * .5f;
    const float displayScale =
        p.levels.peak > 0 ? float(std::min(1000., .9 / p.levels.peak)) : 1.f;
    for (int pixel = 0; pixel < int(size.x); ++pixel) {
        const size_t begin = size_t(pixel / size.x * wave.low.size());
        const size_t end =
            std::min(wave.low.size(),
                     size_t((pixel + 1) / size.x * wave.low.size()) + 1);
        float low = 0, high = 0;
        for (size_t bin = begin; bin < end; ++bin) {
            low = std::min(low, wave.low[bin]);
            high = std::max(high, wave.high[bin]);
        }

        draw->AddLine({origin.x + pixel, mid - high * 42 * displayScale},
                      {origin.x + pixel, mid - low * 42 * displayScale},
                      IM_COL32(233, 161, 70, 255));
    }

    char scaleLabel[64];
    std::snprintf(scaleLabel, sizeof(scaleLabel), "Display x%.1f",
                  displayScale);
    draw->AddText({origin.x + 8, origin.y + 4}, IM_COL32(160, 180, 200, 255),
                  scaleLabel);

    if (p.pcm.loop.end) {
        ImGui::Text("Loop frames [%zu, %zu)", p.pcm.loop.begin, p.pcm.loop.end);
    }
}

void SfxDesigner::draw() {
    if (!visible) {
        return;
    }

    ImGui::SetNextWindowSize({720, 900}, ImGuiCond_FirstUseEver);
    const auto origin = ImGui::GetMainViewport()->WorkPos;
    ImGui::SetNextWindowPos({origin.x + 300, origin.y + 30},
                            ImGuiCond_FirstUseEver);

    if (focus) {
        ImGui::SetNextWindowFocus();
        focus = false;
    }

    if (ImGui::Begin("SFX Designer", &visible)) {
        ImGui::BeginDisabled(s.busy());
        if (ImGui::Button(s.sfxProduct ? "Regenerate" : "Generate")) {
            attempt([&] { s.generateSfx(); });
        }

        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!s.sfxProduct);
        if (ImGui::Button("Play")) {
            attempt([&] { s.playSfx(looping); });
        }

        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            attempt([&] { s.stopSfx(); });
        }

        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Checkbox("Loop Preview", &looping) &&
            s.sfxPreview.handle()) {
            attempt([&] { s.playSfx(looping); });
        }

        if (s.busy()) {
            ImGui::ProgressBar(float(s.progress()), {-90, 0},
                               s.jobKind.c_str());
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                s.cancel();
            }
        }

        if (BASS_GetDevice() == 0) {
            ImGui::TextWrapped("No Sound: generation and export available; "
                               "preview is not audible.");
        }

        ImGui::TextWrapped(
            "Recipe: %s / %s%s", s.sfxDefinition.category.c_str(),
            s.sfxDefinition.id.c_str(), s.sfxDirty ? " * unsaved" : "");
        result();
        ImGui::Separator();
        ImGui::BeginDisabled(s.busy() && s.jobKind == "sfx.load");
        if (ImGui::BeginTabBar("SFX tabs")) {
            if (ImGui::BeginTabItem("Recipes")) {
                browser();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Sound")) {
                sound();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Layers")) {
                layers();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Export")) {
                ImGui::InputText("WAV filename", exportPath,
                                 sizeof(exportPath));
                ImGui::Checkbox("Float32 (otherwise PCM16)", &float32);
                ImGui::BeginDisabled(!s.sfxProduct || s.busy());
                if (ImGui::Button("Export WAV")) {
                    attempt(
                        [&] { s.exportSfx(fs::u8path(exportPath), float32); });
                }

                ImGui::EndDisabled();

                if (ImGui::SliderFloat("Preview volume", &s.sfxVolume, 0, 1) &&
                    s.sfxPreview.handle()) {
                    attempt([&] {
                        s.sfxPreview.controls(s.sfxVolume, 0, s.streamBuffer);
                    });
                }

                ImGui::TextWrapped(
                    "Exports the last generated snapshot, including its loop. "
                    "Existing files are refused; choose a new filename. Save "
                    "Lua in Recipes saves the editable definition separately.");
                ImGui::TextWrapped("Output folder: %s",
                                   s.outputRoot.string().c_str());
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::EndDisabled();

        if (!error.empty()) {
            ImGui::TextWrapped("%s", error.c_str());
        }

        if (!s.error.empty() && s.jobKind.rfind("sfx.", 0) == 0) {
            ImGui::TextWrapped("%s", s.error.c_str());
        }

        if (!s.notice.empty()) {
            ImGui::TextWrapped("%s", s.notice.c_str());
        }
    }

    confirmChanges();
    ImGui::End();
}
} // namespace AudioStudio
