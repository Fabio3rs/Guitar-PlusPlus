#include "session.h"
#include "voice_benchmark.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <png.h>
#include <type_traits>

namespace AudioStudio {
namespace {
constexpr ImU32 amber = IM_COL32(233, 161, 70, 255);
constexpr ImU32 blue = IM_COL32(93, 163, 223, 255);

struct Window {
    GLFWwindow *window = nullptr;
    bool imgui = false, glfwBackend = false, glBackend = false;
    std::string ini;
    explicit Window(const fs::path &output)
        : ini((output / "layout.ini").string()) {
        if (!glfwInit()) {
            throw audio::Error(
                "GLFW initialization failed; use Xvfb or --mode headless");
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        window = glfwCreateWindow(1440, 960, "Guitar++ Audio Studio", nullptr,
                                  nullptr);
        if (!window) {
            glfwTerminate();
            throw audio::Error("Cannot create Audio Studio window");
        }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        imgui = true;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::GetIO().IniFilename = ini.c_str();
        // Optional system font improves readability without adding an asset
        // dependency. ImGui's bundled font remains the portable fallback.
        const fs::path systemFont =
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
        if (fs::exists(systemFont)) {
            ImGui::GetIO().Fonts->AddFontFromFileTTF(
                systemFont.string().c_str(), 16);
        }
        glfwBackend = ImGui_ImplGlfw_InitForOpenGL(window, true);
        glBackend = glfwBackend && ImGui_ImplOpenGL3_Init("#version 110");
        if (!glBackend || !ImGui_ImplOpenGL3_CreateDeviceObjects()) {
            close();
            throw audio::Error("Cannot initialize ImGui OpenGL 2.0 backend");
        }
        ImGui::StyleColorsDark();
        auto &style = ImGui::GetStyle();
        style.WindowRounding = 5;
        style.FrameRounding = 4;
        style.GrabRounding = 4;
        style.WindowPadding = {12, 12};
        style.FramePadding = {8, 5};
        style.ItemSpacing = {8, 7};
        style.Colors[ImGuiCol_WindowBg] = {.075f, .087f, .11f, 1};
        style.Colors[ImGuiCol_ChildBg] = {.09f, .10f, .13f, 1};
        style.Colors[ImGuiCol_FrameBg] = {.15f, .17f, .21f, 1};
        style.Colors[ImGuiCol_Button] = {.21f, .25f, .31f, 1};
        style.Colors[ImGuiCol_ButtonHovered] = {.45f, .32f, .18f, 1};
        style.Colors[ImGuiCol_SliderGrab] = {.92f, .63f, .27f, 1};
        style.Colors[ImGuiCol_CheckMark] = {.92f, .63f, .27f, 1};
    }
    void close() {
        if (glBackend) {
            ImGui_ImplOpenGL3_Shutdown();
            glBackend = false;
        }
        if (glfwBackend) {
            ImGui_ImplGlfw_Shutdown();
            glfwBackend = false;
        }
        if (imgui) {
            ImGui::DestroyContext();
            imgui = false;
        }
        if (window) {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
    }
    ~Window() { close(); }
};

void screenshot(const fs::path &path, GLFWwindow *window) {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    if (w <= 0 || h <= 0) {
        throw audio::Error("Cannot capture minimized window");
    }
    std::vector<unsigned char> pixels(size_t(w) * size_t(h) * 4),
        flipped(pixels.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    for (int y = 0; y < h; ++y) {
        std::memcpy(flipped.data() + size_t(y) * size_t(w) * 4,
                    pixels.data() + size_t(h - y - 1) * size_t(w) * 4,
                    size_t(w) * 4);
    }
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = png_uint_32(w);
    image.height = png_uint_32(h);
    image.format = PNG_FORMAT_RGBA;
    if (!png_image_write_to_file(&image, path.string().c_str(), 0,
                                 flipped.data(), 0, nullptr)) {
        const std::string message = image.message;
        png_image_free(&image);
        throw audio::Error(message);
    }
    png_image_free(&image);
}

bool begins(const std::string &s, const std::string &prefix) {
    return s.rfind(prefix, 0) == 0;
}

std::string labelFor(const std::string &name) {
    const auto dot = name.rfind('.');
    std::string label = name.substr(dot == std::string::npos ? 0 : dot + 1);
    std::string pretty;
    for (char c : label) {
        if (c >= 'A' && c <= 'Z') {
            pretty += ' ';
        }
        pretty += c;
    }
    if (!pretty.empty() && pretty[0] >= 'a' && pretty[0] <= 'z') {
        pretty[0] = char(pretty[0] - 'a' + 'A');
    }
    if (label == "frequency" || label == "rate") {
        pretty += " (Hz)";
    } else if (label == "gain") {
        pretty += " (dB)";
    } else if (label == "cents" || label == "detune") {
        pretty += " (cents)";
    } else if (label == "bandwidth") {
        pretty += " (octaves)";
    } else if (label == "window" || label == "hop" || label == "duration" ||
               label == "attack" || label == "decay" || label == "fadeStart" ||
               label == "loopStart" || label == "loopBody" ||
               label == "release" || label == "crossfade" ||
               label == "preRoll" || label == "tail" || label == "fade") {
        pretty += " (s)";
    }
    return pretty;
}

bool parameters(audio::Settings &settings,
                const std::function<bool(const std::string &)> &filter) {
    bool changed = false;
    ImGui::PushItemWidth(-150);
    audio::visitSettings(settings, [&](const std::string &name, auto &value,
                                       double low, double high) {
        if (!filter(name)) {
            return;
        }
        const auto label = labelFor(name) + "##" + name;
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, bool>) {
            changed |= ImGui::Checkbox(label.c_str(), &value);
        } else if constexpr (std::is_integral_v<T>) {
            if (name == "analysis.fft") {
                int index = 0;
                for (int n = 512; n < value; n *= 2) {
                    ++index;
                }
                if (ImGui::Combo(
                        label.c_str(), &index,
                        "512\0 1024\0 2048\0 4096\0 8192\0 16384\0 32768\0")) {
                    value = 512 << index;
                    changed = true;
                }
            } else {
                changed |= ImGui::DragInt(label.c_str(), &value, 1, int(low),
                                          int(high));
            }
        } else {
            const ImGuiSliderFlags flags = low > 0 && high / low > 100
                                               ? ImGuiSliderFlags_Logarithmic
                                               : ImGuiSliderFlags_None;
            changed |= ImGui::SliderScalar(label.c_str(), ImGuiDataType_Double,
                                           &value, &low, &high, "%.4f", flags);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "%s\nRange: %.4g .. %.4g\nDrag or Ctrl+click to enter a value.",
                name.c_str(), low, high);
        }
    });
    ImGui::PopItemWidth();
    return changed;
}

struct UI {
    Session &s;
    char file[4096] = {}, root[4096] = {}, preset[1024] = "preset.json",
         exportFile[1024] = "preview.wav";
    std::vector<fs::path> files;
    int fxTarget = 1, previewType = 3;
    VoiceBenchmarkOptions voiceOptions;
    char voiceFile[1024] = "voice-comparison.json";
    size_t selectedRegion = 0;
    char cachePath[1024] = "bank.gppfc", analysisPath[1024] = "analysis.json";
    char cacheExport[1024] = "adaptive-fail",
         genericPath[4096] = "build/bin/data/sounds";
    int adaptiveMode = 0, releaseMs = 20, adaptiveTrials = 30;
    bool held[5] = {};
    int benchmarkIterations = 30;
    char benchmarkFile[1024] = "benchmark.json";
    bool float32 = false, segment = false, rawExport = false, first = true,
         focusSynth = false;
    double zoom = 1, viewStart = 0;
    audio::DeviceSettings deviceDraft;
    std::string localError;
    explicit UI(Session &session) : s(session), deviceDraft(s.deviceSettings) {
        std::snprintf(root, sizeof(root), "%s",
                      (fs::path(GPP_AUDIO_WORKSPACE) / "build/bin/data/songs")
                          .string()
                          .c_str());
        std::snprintf(file, sizeof(file), "%s", s.source.string().c_str());
        attempt([&] { scan(); });
    }
    template <class F> void attempt(F fn) {
        try {
            fn();
            localError.clear();
        } catch (const std::exception &e) {
            localError = e.what();
        }
    }
    void scan() {
        files.clear();
        std::error_code error;
        fs::recursive_directory_iterator iterator(
            root, fs::directory_options::skip_permission_denied, error),
            end;
        if (error) {
            throw audio::Error(
                "Song folder unavailable; enter another folder or open a file");
        }
        while (iterator != end) {
            if (iterator->is_regular_file(error)) {
                auto ext = iterator->path().extension().string();
                std::transform(
                    ext.begin(), ext.end(), ext.begin(),
                    [](unsigned char c) { return char(std::tolower(c)); });
                if (ext == ".ogg" || ext == ".mp3" || ext == ".wav" ||
                    ext == ".aiff" || ext == ".mp2") {
                    files.push_back(iterator->path());
                }
            }
            iterator.increment(error);
            if (error) {
                break;
            }
        }
        std::sort(files.begin(), files.end());
    }
    void layout() {
        const auto dock =
            ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        if (first && !fs::exists(s.outputRoot / "layout.ini")) {
            focusSynth = true;
            ImGui::DockBuilderRemoveNode(dock);
            ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dock,
                                          ImGui::GetMainViewport()->WorkSize);
            ImGuiID center = dock;
            const auto left = ImGui::DockBuilderSplitNode(
                center, ImGuiDir_Left, .20f, nullptr, &center);
            const auto right = ImGui::DockBuilderSplitNode(
                center, ImGuiDir_Right, .39f, nullptr, &center);
            const auto bottom = ImGui::DockBuilderSplitNode(
                center, ImGuiDir_Down, .32f, nullptr, &center);
            const auto spectrum = ImGui::DockBuilderSplitNode(
                center, ImGuiDir_Down, .44f, nullptr, &center);
            ImGui::DockBuilderDockWindow("Sources", left);
            ImGui::DockBuilderDockWindow("Timeline / Waveform", center);
            ImGui::DockBuilderDockWindow("FFT Analysis", spectrum);
            ImGui::DockBuilderDockWindow("Frets / Preview", bottom);
            ImGui::DockBuilderDockWindow("Fail Synth", right);
            ImGui::DockBuilderDockWindow("BASS FX", right);
            ImGui::DockBuilderDockWindow("Audio Device", right);
            ImGui::DockBuilderDockWindow("Diagnostics", right);
            ImGui::DockBuilderDockWindow("Whole Song Analysis", right);
            ImGui::DockBuilderFinish(dock);
        }
        first = false;
    }
    void explorer() {
        if (ImGui::Begin("Sources")) {
            ImGui::TextColored({.94f, .67f, .32f, 1}, "GUITAR++ AUDIO STUDIO");
            ImGui::TextDisabled("Analysis & synthesis workbench");
            ImGui::Separator();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##file", "Audio file path", file,
                                     sizeof(file));
            if (ImGui::Button("Open audio", {-1, 0})) {
                attempt([&] { s.load(file); });
            }
            ImGui::TextDisabled("Drop an audio file into this window");
            ImGui::Spacing();
            ImGui::TextUnformatted("Song folder");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##root", root, sizeof(root));
            if (ImGui::Button("Refresh files")) {
                attempt([&] { scan(); });
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%zu files", files.size());
            ImGui::BeginChild(
                "files",
                {0, std::max(80.f, ImGui::GetContentRegionAvail().y - 205)},
                ImGuiChildFlags_Borders);
            for (const auto &path : files) {
                const auto label =
                    path.lexically_relative(fs::path(root)).string();
                if (ImGui::Selectable(label.c_str(), path == s.source)) {
                    std::snprintf(file, sizeof(file), "%s",
                                  path.string().c_str());
                    attempt([&] { s.load(path); });
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", path.string().c_str());
                }
            }
            ImGui::EndChild();
            ImGui::SeparatorText("Presets");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##preset", preset, sizeof(preset));
            if (ImGui::Button("Save preset")) {
                attempt([&] { savePreset(s.artifact(preset), s.settings); });
            }
            ImGui::SameLine();
            if (ImGui::Button("Load preset")) {
                attempt([&] {
                    s.updateSettings(loadPreset(fs::path(preset).is_absolute()
                                                    ? fs::path(preset)
                                                    : s.outputRoot / preset));
                });
            }
            if (ImGui::Button("Reset parameters")) {
                attempt([&] { s.updateSettings({}); });
            }
            ImGui::TextWrapped("Output: %s", s.outputRoot.string().c_str());
        }
        ImGui::End();
    }
    void timeline() {
        if (ImGui::Begin("Timeline / Waveform")) {
            if (!s.wave) {
                ImGui::TextWrapped("Open a song or stem to inspect its "
                                   "waveform and generate a fail sound.");
            } else {
                ImGui::TextUnformatted(s.source.filename().string().c_str());
                ImGui::TextDisabled("%u Hz  |  %u channels  |  %.3f s",
                                    s.wave->info.rate, s.wave->info.channels,
                                    s.wave->info.duration);
                ImGui::SameLine();
                ImGui::TextColored({.43f, .82f, .61f, 1}, "%s",
                                   s.busy() ? "Processing" : "Ready");
                const double duration = s.wave->info.duration;
                const double span = duration / zoom;
                viewStart =
                    std::clamp(viewStart, 0., std::max(0., duration - span));
                const ImVec2 origin = ImGui::GetCursorScreenPos();
                const ImVec2 size{
                    std::max(1.f, ImGui::GetContentRegionAvail().x),
                    std::max(100.f, ImGui::GetContentRegionAvail().y - 165)};
                ImGui::InvisibleButton("waveform", size);
                auto *draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(origin,
                                    {origin.x + size.x, origin.y + size.y},
                                    IM_COL32(14, 19, 26, 255), 5);
                const float mid = origin.y + size.y * .5f;
                const auto timeX = [&](double t) {
                    return origin.x + float((t - viewStart) / span) * size.x;
                };
                const float selL = std::clamp(timeX(s.selectionStart), origin.x,
                                              origin.x + size.x);
                const float selR = std::clamp(timeX(s.selectionEnd), origin.x,
                                              origin.x + size.x);
                draw->AddRectFilled({selL, origin.y}, {selR, origin.y + size.y},
                                    IM_COL32(200, 145, 70, 35));
                for (int pixel = 0; pixel < int(size.x); ++pixel) {
                    size_t lo = size_t((viewStart + span * pixel / size.x) /
                                       duration * s.wave->low.size());
                    size_t hi =
                        size_t((viewStart + span * (pixel + 1) / size.x) /
                               duration * s.wave->low.size()) +
                        1;
                    hi = std::min(hi, s.wave->low.size());
                    float low = 0, high = 0;
                    for (size_t bin = lo; bin < hi; ++bin) {
                        low = std::min(low, s.wave->low[bin]);
                        high = std::max(high, s.wave->high[bin]);
                    }
                    draw->AddLine(
                        {origin.x + pixel, mid - high * size.y * .42f},
                        {origin.x + pixel, mid - low * size.y * .42f}, amber);
                }
                for (int tick = 0; tick < 5; ++tick) {
                    char label[64];
                    std::snprintf(label, sizeof(label), "%.2f s",
                                  viewStart + span * tick / 4);
                    draw->AddText({origin.x + float(tick) * (size.x - 65) / 4,
                                   origin.y + size.y - 20},
                                  blue, label);
                }
                const auto *song = s.songAnalysis ? s.songAnalysis.get()
                                   : s.failCache  ? &s.failCache->song
                                                  : nullptr;
                if (song) {
                    for (size_t i = 0; i < song->regions.size(); ++i) {
                        const auto &region = song->regions[i];
                        const float l = timeX(double(region.begin) /
                                              song->info.rate),
                                    r = timeX(double(region.end) /
                                              song->info.rate);
                        if (r < origin.x || l > origin.x + size.x) {
                            continue;
                        }
                        ImVec4 color;
                        ImGui::ColorConvertHSVtoRGB(
                            float((region.setId * 0.61803398875)), .5f, .8f,
                            color.x, color.y, color.z);
                        color.w = .65f;
                        if (region.silent) {
                            color = {.35f, .35f, .35f, .8f};
                        }
                        const float left = std::clamp(l, origin.x,
                                                      origin.x + size.x),
                                    right = std::clamp(r, origin.x,
                                                       origin.x + size.x);
                        draw->AddRectFilled(
                            {left, origin.y}, {right, origin.y + 26},
                            ImGui::ColorConvertFloat4ToU32(color));
                        draw->AddLine({left, origin.y},
                                      {left, origin.y + size.y},
                                      IM_COL32(180, 190, 210, 100));
                        const float representative = timeX(
                            double(region.representative) / song->info.rate);
                        if (representative >= origin.x &&
                            representative <= origin.x + size.x) {
                            draw->AddLine({representative, origin.y + 28},
                                          {representative, origin.y + 42},
                                          IM_COL32(255, 235, 150, 255), 2);
                        }
                        if (right - left > 40) {
                            char text[48];
                            std::snprintf(text, sizeof(text), "%zu/S%u", i,
                                          region.setId);
                            draw->AddText({left + 3, origin.y + 4},
                                          IM_COL32(255, 255, 255, 255), text);
                        }
                        if (i == selectedRegion) {
                            draw->AddRect({left, origin.y},
                                          {right, origin.y + 26},
                                          IM_COL32(255, 255, 255, 255));
                        }
                    }
                }
                const float cursorX = timeX(s.cursor);
                if (cursorX >= origin.x && cursorX <= origin.x + size.x) {
                    draw->AddLine({cursorX, origin.y},
                                  {cursorX, origin.y + size.y},
                                  IM_COL32(245, 246, 249, 255), 2);
                }
                const float playX = timeX(s.music.position());
                if (playX >= origin.x && playX <= origin.x + size.x) {
                    draw->AddLine({playX, origin.y}, {playX, origin.y + size.y},
                                  blue);
                }
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0)) {
                    s.cursor = std::clamp(
                        viewStart +
                            double(ImGui::GetIO().MousePos.x - origin.x) /
                                size.x * span,
                        0., std::max(0., duration - .0001));
                }
                if (ImGui::IsItemDeactivated() ||
                    (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(0))) {
                    const double time = std::clamp(
                        viewStart +
                            double(ImGui::GetIO().MousePos.x - origin.x) /
                                size.x * span,
                        0., std::max(0., duration - .0001));
                    attempt([&] {
                        if (song && ImGui::GetIO().MousePos.y < origin.y + 28) {
                            const uint64_t frame =
                                uint64_t(time * song->info.rate);
                            for (size_t i = 0; i < song->regions.size(); ++i) {
                                if (frame >= song->regions[i].begin &&
                                    frame < song->regions[i].end) {
                                    selectedRegion = i;
                                    break;
                                }
                            }
                            s.seek(double(song->regions[selectedRegion]
                                              .representative) /
                                   song->info.rate);
                        } else {
                            s.seek(time);
                            if (!s.busy()) {
                                s.generate(false);
                            }
                        }
                    });
                }
                ImGui::PushItemWidth(95);
                double cursor = s.cursor;
                if (ImGui::InputDouble("Cursor (s)", &cursor, 0, 0, "%.3f",
                                       ImGuiInputTextFlags_EnterReturnsTrue)) {
                    attempt([&] {
                        s.seek(cursor);
                        if (!s.busy()) {
                            s.generate(false);
                        }
                    });
                }
                ImGui::SameLine();
                if (ImGui::Button("Play")) {
                    attempt([&] { s.music.play(); });
                }
                ImGui::SameLine();
                if (ImGui::Button("Pause")) {
                    attempt([&] { s.music.pause(); });
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop")) {
                    attempt([&] { s.music.stop(); });
                }
                double selectionStart = s.selectionStart;
                double selectionEnd = s.selectionEnd;
                bool selectionChanged = ImGui::InputDouble(
                    "Start (s)", &selectionStart, 0, 0, "%.3f",
                    ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::SameLine();
                selectionChanged |=
                    ImGui::InputDouble("End (s)", &selectionEnd, 0, 0, "%.3f",
                                       ImGuiInputTextFlags_EnterReturnsTrue);
                if (selectionChanged) {
                    attempt(
                        [&] { s.setSelection(selectionStart, selectionEnd); });
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Press Enter to apply selection bounds");
                }
                bool loop = s.musicLoop;
                if (ImGui::Checkbox("Repeat selection", &loop)) {
                    attempt([&] {
                        s.setMusicLoop(s.selectionStart, s.selectionEnd, loop);
                    });
                }
                ImGui::SameLine();
                const double minZoom = 1, maxZoom = 100;
                ImGui::SliderScalar("Zoom", ImGuiDataType_Double, &zoom,
                                    &minZoom, &maxZoom, "%.1fx");
                const double zero = 0, last = std::max(0., duration - span);
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderScalar("##scroll", ImGuiDataType_Double,
                                    &viewStart, &zero, &last,
                                    "View starts at %.3f s");
                ImGui::PopItemWidth();
            }
            if (s.busy()) {
                ImGui::ProgressBar(float(s.progress()), {-90, 0},
                                   s.jobKind.c_str());
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    s.cancel();
                }
            }
        }
        ImGui::End();
    }
    void spectrum() {
        if (ImGui::Begin("FFT Analysis")) {
            if (s.product && !s.product->analysis.spectrum.empty()) {
                const auto &values = s.product->analysis.spectrum;
                const auto pos = ImGui::GetCursorScreenPos();
                const ImVec2 size{
                    std::max(1.f, ImGui::GetContentRegionAvail().x), 85};
                ImGui::InvisibleButton("spectrum", size);
                auto *draw = ImGui::GetWindowDrawList();
                const float maximum = std::max(
                    1e-9f, *std::max_element(values.begin(), values.end()));
                const int bins = 96;
                for (int i = 0; i < bins; ++i) {
                    size_t lo = size_t(
                        std::pow(double(values.size()), double(i) / bins));
                    size_t hi = std::min(
                        values.size(), size_t(std::pow(double(values.size()),
                                                       double(i + 1) / bins)) +
                                           1);
                    lo = std::min(lo, values.size() - 1);
                    const float value = *std::max_element(values.begin() + lo,
                                                          values.begin() + hi);
                    draw->AddRectFilled(
                        {pos.x + i * size.x / bins,
                         pos.y + size.y * (1 - value / maximum)},
                        {pos.x + (i + 1) * size.x / bins - 1, pos.y + size.y},
                        blue);
                }
                ImGui::TextDisabled(
                    "Log frequency | %.3f s | %.2f Hz/bin | %.2f ms analysis",
                    s.product->time,
                    double(s.product->analysis.rate) /
                        s.product->settings.analysis.fft,
                    s.product->analysis.milliseconds);
                if (ImGui::BeginTable("notes", 4,
                                      ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_BordersInnerH)) {
                    for (const char *name : {"Note", "MIDI", "Hz", "Score"}) {
                        ImGui::TableSetupColumn(name);
                    }
                    ImGui::TableHeadersRow();
                    for (const auto &note : s.product->analysis.notes) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(note.name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%d", note.midi);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.2f", note.frequency);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.5f", note.score);
                    }
                    ImGui::EndTable();
                }
            }
            if (ImGui::CollapsingHeader("Analysis parameters")) {
                auto next = s.settings;
                if (parameters(next, [](const std::string &name) {
                        return begins(name, "analysis.");
                    })) {
                    attempt([&] { s.updateSettings(next); });
                }
            }
            if (ImGui::Button("Analyze cursor")) {
                attempt([&] { s.generate(false); });
            }
        }
        ImGui::End();
    }
    void synth() {
        if (ImGui::Begin("Fail Synth")) {
            ImGui::TextWrapped(
                "Generate reusable PCM from the selected harmonic context.");
            auto next = s.settings;
            bool changed = false;
            if (ImGui::Button("51 ms")) {
                next.synth.duration = .051;
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("100 ms")) {
                next.synth.duration = .1;
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("850 ms")) {
                next.synth.duration = .85;
                changed = true;
            }
            if (ImGui::CollapsingHeader("Timing / envelope",
                                        ImGuiTreeNodeFlags_DefaultOpen)) {
                changed |= parameters(next, [](const std::string &n) {
                    return begins(n, "synth.") &&
                           (n == "synth.duration" || n == "synth.attack" ||
                            n == "synth.decay" || n == "synth.fadeStart" ||
                            n == "synth.sustain" || n == "synth.release" ||
                            n == "synth.rate");
                });
            }
            if (ImGui::CollapsingHeader("Loop / crossfade",
                                        ImGuiTreeNodeFlags_DefaultOpen)) {
                changed |= parameters(next, [](const std::string &n) {
                    return begins(n, "synth.loop") || n == "synth.crossfade";
                });
                ImGui::TextDisabled(
                    "Baked loop excludes release; Stop ends immediately.");
            }
            if (ImGui::CollapsingHeader("Oscillator / pick / voicing")) {
                changed |= parameters(next, [](const std::string &n) {
                    return begins(n, "synth.") &&
                           (n == "synth.voices" ||
                            begins(n, "synth.harmonic") ||
                            begins(n, "synth.transient") ||
                            begins(n, "synth.bend") ||
                            n == "synth.saturation" || n == "synth.detune" ||
                            n == "synth.spread");
                });
            }
            if (ImGui::CollapsingHeader(
                    "Frets: semitone shifts / linear gains")) {
                changed |= parameters(next, [](const std::string &n) {
                    return begins(n, "synth.shift") || begins(n, "synth.gain");
                });
            }
            if (ImGui::CollapsingHeader("Output normalization")) {
                changed |= parameters(next, [](const std::string &n) {
                    return n == "synth.normalize" || n == "synth.rms" ||
                           n == "synth.peak";
                });
            }
            if (changed) {
                attempt([&] { s.updateSettings(next); });
            }
            ImGui::Separator();
            if (ImGui::Button("Generate preview", {-1, 34})) {
                attempt([&] { s.generate(true); });
            }
            ImGui::TextWrapped(
                "Content duration and FX tail are independent. Changed "
                "synthesis parameters require regeneration.");
        }
        ImGui::End();
    }
    void fx() {
        if (ImGui::Begin("BASS FX")) {
            ImGui::Combo("Target", &fxTarget, "Music / stem\0Synth preview\0");
            ImGui::TextDisabled("Pre EQ  >  Distortion  >  Post EQ");
            auto next = s.settings;
            if (fxTarget == 0) {
                next.fx = s.musicFx;
            }
            bool changed = parameters(
                next, [](const std::string &n) { return n == "fx.enabled"; });
            if (ImGui::CollapsingHeader("Pre EQ",
                                        ImGuiTreeNodeFlags_DefaultOpen)) {
                changed |= parameters(next, [](const std::string &n) {
                    return begins(n, "fx.pre.");
                });
            }
            if (ImGui::CollapsingHeader("Distortion",
                                        ImGuiTreeNodeFlags_DefaultOpen)) {
                changed |= parameters(next, [](const std::string &n) {
                    return n == "fx.distortion" || n == "fx.drive" ||
                           n == "fx.dry" || n == "fx.wet" ||
                           n == "fx.feedback" || n == "fx.volume";
                });
            }
            if (ImGui::CollapsingHeader("Post EQ",
                                        ImGuiTreeNodeFlags_DefaultOpen)) {
                changed |= parameters(next, [](const std::string &n) {
                    return begins(n, "fx.post.");
                });
            }
            if (ImGui::CollapsingHeader("Offline padding / normalization")) {
                changed |= parameters(next, [](const std::string &n) {
                    return n == "fx.preRoll" || n == "fx.tail" ||
                           n == "fx.fade" || n == "fx.normalize" ||
                           n == "fx.rms" || n == "fx.peak";
                });
            }
            ImGui::TextWrapped(
                "Normalization/padding apply only when rendering PCM. Live FX "
                "updates may be delayed by playback buffers.");
            if (changed) {
                attempt([&] {
                    if (fxTarget == 0) {
                        s.applyMusicFx(next.fx);
                    } else {
                        s.updateSettings(next);
                    }
                });
            }
        }
        ImGui::End();
    }
    void preview() {
        if (ImGui::Begin("Frets / Preview")) {
            const ImVec4 colors[] = {{.22f, .72f, .40f, 1},
                                     {.87f, .31f, .34f, 1},
                                     {.94f, .79f, .29f, 1},
                                     {.32f, .60f, .88f, 1},
                                     {.95f, .58f, .23f, 1}};
            const char *names[] = {"G", "R", "Y", "B", "O"};
            const float width =
                std::max(30.f, (ImGui::GetContentRegionAvail().x - 32) / 5);
            for (int i = 0; i < 5; ++i) {
                if (i) {
                    ImGui::SameLine();
                }
                ImVec4 color = colors[i];
                if (!(s.settings.synth.fretMask & (1 << i))) {
                    color.w = .45f;
                }
                ImGui::PushStyleColor(ImGuiCol_Button, color);
                ImGui::PushStyleColor(ImGuiCol_Text, {0, 0, 0, 1});
                if (ImGui::Button(names[i], {width, 34})) {
                    auto next = s.settings;
                    next.synth.fretMask ^= 1 << i;
                    attempt([&] { s.updateSettings(next); });
                }
                ImGui::PopStyleColor(2);
            }
            ImGui::SetNextItemWidth(160);
            ImGui::Combo("##preview", &previewType,
                         "Original region\0Raw synth\0Live FX\0Rendered PCM\0");
            ImGui::SameLine();
            if (ImGui::Button("Trigger")) {
                static const char *modes[] = {"original", "raw", "live",
                                              "processed"};
                attempt([&] { s.playPreview(modes[previewType]); });
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop preview")) {
                attempt([&] { s.preview.stop(); });
            }
            ImGui::SameLine();
            if (ImGui::Button("Generate")) {
                attempt([&] { s.generate(true); });
            }
            if (s.product && encode(settingsJson(s.product->settings)) !=
                                 encode(settingsJson(s.settings))) {
                ImGui::TextColored(
                    {1, .7f, .3f, 1},
                    "Edited parameters: generate to refresh rendered PCM.");
            }
            ImGui::PushItemWidth(100);
            bool musicChanged =
                ImGui::SliderFloat("Music", &s.musicVolume, 0, 1, "%.2f");
            ImGui::SameLine();
            bool previewChanged =
                ImGui::SliderFloat("Preview", &s.previewVolume, 0, 1, "%.2f");
            musicChanged |=
                ImGui::SliderFloat("Music pan", &s.musicPan, -1, 1, "%.2f");
            ImGui::SameLine();
            previewChanged |=
                ImGui::SliderFloat("Preview pan", &s.previewPan, -1, 1, "%.2f");
            ImGui::PopItemWidth();
            if (musicChanged && s.music.handle()) {
                attempt([&] {
                    s.music.controls(s.musicVolume, s.musicPan, s.streamBuffer);
                });
            }
            if (previewChanged && s.preview.handle()) {
                attempt([&] {
                    s.preview.controls(s.previewVolume, s.previewPan,
                                       s.streamBuffer);
                });
            }
            for (int slot = 0; slot < 2; ++slot) {
                const std::string label = slot == 0 ? "A" : "B";
                if (ImGui::Button(("Store " + label).c_str())) {
                    attempt([&] { s.storeSlot(slot); });
                }
                ImGui::SameLine();
                if (ImGui::Button(("Hear " + label).c_str())) {
                    attempt([&] {
                        s.recallSlot(slot);
                        s.playPreview("processed");
                    });
                }
                if (slot == 0) {
                    ImGui::SameLine();
                }
            }
            if (ImGui::CollapsingHeader("Export WAV")) {
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##export", exportFile, sizeof(exportFile));
                ImGui::Checkbox("Source selection", &segment);
                ImGui::SameLine();
                ImGui::Checkbox("Without FX", &rawExport);
                ImGui::Checkbox("Float32 (otherwise PCM16)", &float32);
                if (ImGui::Button("Export")) {
                    attempt([&] {
                        s.exportAudio(exportFile, segment, rawExport, float32);
                    });
                }
            }
        }
        ImGui::End();
    }
    void device() {
        if (ImGui::Begin("Audio Device")) {
            ImGui::TextWrapped(
                "Device changes stop playback and rebuild channels. Analysis "
                "and presets remain available without an output device.");
            if (ImGui::BeginCombo("Device",
                                  std::to_string(deviceDraft.device).c_str())) {
                if (ImGui::Selectable("-1: System default",
                                      deviceDraft.device == -1)) {
                    deviceDraft.device = -1;
                }
                for (const auto &d : audio::devices()) {
                    if (ImGui::Selectable(
                            (std::to_string(d.id) + ": " + d.name).c_str(),
                            deviceDraft.device == d.id)) {
                        deviceDraft.device = d.id;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::InputInt("Requested rate (Hz)", &deviceDraft.rate);
            ImGui::InputInt("Channel buffer (ms)", &deviceDraft.bufferMs);
            ImGui::InputInt("Channel update (ms)", &deviceDraft.updateMs);
            ImGui::InputInt("Device buffer (ms)", &deviceDraft.deviceBufferMs);
            ImGui::InputInt("Device period (ms)", &deviceDraft.devicePeriodMs);
            if (ImGui::Button("Apply device settings")) {
                attempt([&] {
                    s.applyDevice(deviceDraft);
                    deviceDraft = s.deviceSettings;
                });
            }
            ImGui::SeparatorText("Live channel buffering");
            if (ImGui::SliderFloat("Buffer (s)", &s.streamBuffer, 0,
                                   float(s.deviceSettings.bufferMs) / 1000,
                                   "%.3f")) {
                attempt([&] {
                    if (s.music.handle()) {
                        s.music.controls(s.musicVolume, s.musicPan,
                                         s.streamBuffer);
                    }
                    if (s.preview.handle()) {
                        s.preview.controls(s.previewVolume, s.previewPan,
                                           s.streamBuffer);
                    }
                });
            }
            ImGui::TextWrapped(
                "Zero removes channel buffering. Device and system buffering "
                "still apply. Smaller buffers increase deadline pressure.");
            BASS_INFO info{};
            if (BASS_GetInfo(&info)) {
                ImGui::Text("Actual device: %u Hz / %u speakers\nReported "
                            "minimum buffer: %u ms",
                            info.freq, info.speakers, info.minbuf);
            }
        }
        ImGui::End();
    }
    void diagnostics() {
        if (ImGui::Begin("Diagnostics")) {
            ImGui::Text("BASS %08x | FX %08x", BASS_GetVersion(),
                        BASS_FX_GetVersion());
            ImGui::Text("BASS CPU %.2f%% | UI %.1f FPS", double(BASS_GetCPU()),
                        double(ImGui::GetIO().Framerate));
            ImGui::Text("Device %u: %s", BASS_GetDevice(),
                        BASS_GetDevice() == 0 ? "No Sound" : "Playback output");
            ImGui::Text("Seek API %.3f ms | Play API %.3f ms",
                        s.music.lastSeekMs, s.preview.lastPlayMs);
            ImGui::Text("Preview preparation %.3f ms", s.previewPrepareMs);
            if (s.music.loopError() || s.preview.loopError()) {
                ImGui::Text("Loop callback errors: music %d, preview %d",
                            s.music.loopError(), s.preview.loopError());
            }
            ImGui::TextWrapped(
                "Audible latency: not measured. API duration and buffer "
                "configuration are not loopback measurements.");
            ImGui::SeparatorText("Benchmark / JSON report");
            ImGui::InputInt("Iterations", &benchmarkIterations);
            ImGui::InputText("Report filename", benchmarkFile,
                             sizeof(benchmarkFile));
            ImGui::TextWrapped("Muted API/preparation trials. First run is "
                               "separate; warm runs report p50/p95/p99. Pause "
                               "other playback for comparisons.");
            if (ImGui::Button("Run benchmark and export")) {
                attempt(
                    [&] { s.benchmark(benchmarkIterations, benchmarkFile); });
            }
            if (s.busy() && s.jobKind == "benchmark") {
                ImGui::ProgressBar(float(s.progress()));
                if (ImGui::Button("Cancel benchmark")) {
                    s.cancel();
                }
            }
            if (s.benchmarkReport) {
                ImGui::TextWrapped("Last completed report: %s",
                                   (*s.benchmarkReport)["source"].GetString());
                const auto &metrics = (*s.benchmarkReport)["metrics"];
                for (auto it = metrics.MemberBegin(); it != metrics.MemberEnd();
                     ++it) {
                    ImGui::Text("%s: p50 %.3f / p95 %.3f ms",
                                it->name.GetString(),
                                it->value["p50_ms"].GetDouble(),
                                it->value["p95_ms"].GetDouble());
                }
                ImGui::TextWrapped(
                    "Pending: final-mix probe, system loopback, physical "
                    "output and glitch/overlap tests. NoSound results describe "
                    "processing/API cost only.");
            }
            ImGui::SeparatorText("HSAMPLE vs shared PCM streams");
            ImGui::InputInt("Maximum voices", &voiceOptions.voices);
            ImGui::InputInt("Measured bursts", &voiceOptions.bursts);
            ImGui::InputInt("Trigger interval (ms)", &voiceOptions.intervalMs);
            ImGui::Checkbox("Replace oldest at capacity",
                            &voiceOptions.stealOldest);
            ImGui::Checkbox("Trim FX tail to raw duration",
                            &voiceOptions.trimTail);
            ImGui::InputText("Voice report", voiceFile, sizeof(voiceFile));
            ImGui::TextWrapped(
                "Uses the last generated one-shot PCM. Each burst attempts "
                "twice the voice limit; each strategy/count runs in a fresh "
                "process. Muted playback; pause other audio for comparisons.");
            if (ImGui::Button("Compare HSAMPLE / streams")) {
                attempt([&] { s.compare(voiceOptions, voiceFile); });
            }
            ImGui::SameLine();
            if (ImGui::Button("Open previous report")) {
                attempt([&] {
                    fs::path path(voiceFile);
                    if (path.is_relative()) {
                        path = s.outputRoot / path;
                    }
                    s.voiceReport =
                        std::make_shared<Json>(loadVoiceReport(path));
                });
            }
            if (s.busy() && s.jobKind == "compare_voices") {
                ImGui::ProgressBar(float(s.progress()));
                if (ImGui::Button("Cancel comparison")) {
                    s.cancel();
                }
            }
            if (s.voiceReport) {
                ImGui::TextWrapped(
                    "Last completed/imported report. Its recorded settings and "
                    "device may differ from the current session. RSS is "
                    "process memory, not exact BASS allocation.");
            }
            if (s.voiceReport &&
                ImGui::BeginTable("Voice comparison", 6,
                                  ImGuiTableFlags_Borders |
                                      ImGuiTableFlags_ScrollX)) {
                for (const char *label :
                     {"Strategy", "Voices", "Acquire p95 ms", "Play p95 ms",
                      "RSS growth KiB", "Rejected / replaced"}) {
                    ImGui::TableSetupColumn(label);
                }
                ImGui::TableHeadersRow();
                for (const auto &c : (*s.voiceReport)["cases"].GetArray()) {
                    const auto &summary = c["summary"];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(c["strategy"].GetString());
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", c["voices"].GetInt());
                    for (const char *key : {"acquire_p95_ms", "play_p95_ms",
                                            "rss_growth_bytes"}) {
                        ImGui::TableNextColumn();
                        if (summary[key].IsNull()) {
                            ImGui::TextUnformatted("unavailable");
                        } else {
                            ImGui::Text("%.4f", summary[key].GetDouble() /
                                                    (std::string(key) ==
                                                             "rss_growth_bytes"
                                                         ? 1024
                                                         : 1));
                        }
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text("%.0f / %.0f",
                                summary["rejected"].IsNumber()
                                    ? summary["rejected"].GetDouble()
                                    : 0,
                                summary["replaced"].IsNumber()
                                    ? summary["replaced"].GetDouble()
                                    : 0);
                }
                ImGui::EndTable();
            }
            if (s.product) {
                const auto &p = *s.product;
                ImGui::SeparatorText("Current result");
                ImGui::Text("Analysis %.2f ms | Synthesis %.2f ms | FX %.2f ms",
                            p.analysis.milliseconds, p.synthMs, p.fxMs);
                ImGui::Text(
                    "PCM %.2f MiB | Raw %.5f s | Processed %.5f s",
                    double((p.raw.samples.size() + p.processed.samples.size()) *
                           4) /
                        1048576,
                    p.raw.seconds(), p.processed.seconds());
                ImGui::Text("Peak %.4f | RMS %.4f | Clipped %zu",
                            p.processedLevels.peak, p.processedLevels.rms,
                            p.processedLevels.clipped);
                ImGui::Text("Loop frames [%zu, %zu)", p.processed.loop.begin,
                            p.processed.loop.end);
                if (encode(settingsJson(p.settings)) !=
                    encode(settingsJson(s.settings))) {
                    ImGui::TextColored({1, .7f, .3f, 1},
                                       "Parameters changed: regenerate to "
                                       "update rendered PCM.");
                }
            }
            if (!s.notice.empty()) {
                ImGui::TextWrapped("%s", s.notice.c_str());
            }
        }
        ImGui::End();
    }
    void adaptive() {
        if (ImGui::Begin("Whole Song Analysis")) {
            auto job = [&](const char *kind, const fs::path &path = {}) {
                attempt([&] { s.adaptiveJob(kind, path, adaptiveTrials); });
            };
            ImGui::TextWrapped(
                "Analyze the loaded song or stem. Cache synthesis uses its own "
                "settings (51 ms content by default).");
            if (ImGui::Button("Scan whole song")) {
                job("song.scan");
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel##song")) {
                s.cancel();
            }
            if (s.busy()) {
                ImGui::ProgressBar(float(s.progress()), {-1, 0},
                                   s.jobKind.c_str());
            }
            if (ImGui::CollapsingHeader("Detection parameters")) {
                auto settings = s.songSettings;
                bool changed = false;
                audio::visitSongSettings(settings, [&](const std::string &name,
                                                       auto &v, double low,
                                                       double high) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, bool>) {
                        changed |= ImGui::Checkbox(name.c_str(), &v);
                    } else if constexpr (std::is_integral_v<T>) {
                        int index = 0;
                        for (int n = 512; n < v; n *= 2) {
                            ++index;
                        }
                        if (ImGui::Combo(name.c_str(), &index,
                                         "512\0 1024\0 2048\0 4096\0 8192\0 "
                                         "16384\0 32768\0")) {
                            v = 512 << index;
                            changed = true;
                        }
                    } else {
                        changed |= ImGui::SliderScalar(name.c_str(),
                                                       ImGuiDataType_Double, &v,
                                                       &low, &high, "%.3f");
                    }
                });
                if (changed) {
                    attempt(
                        [&] { s.updateAdaptive(settings, s.cacheSettings); });
                }
            }
            if (ImGui::Button("Resegment stored observations")) {
                job("song.segment");
            }
            if (ImGui::CollapsingHeader("Cache synthesis / FX")) {
                auto settings = s.cacheSettings;
                if (parameters(settings,
                               [](const std::string &) { return true; })) {
                    attempt(
                        [&] { s.updateAdaptive(s.songSettings, settings); });
                }
                if (ImGui::Button("Copy manual synth settings")) {
                    attempt(
                        [&] { s.updateAdaptive(s.songSettings, s.settings); });
                }
            }
            if (ImGui::Button("Generate five per set")) {
                job("cache.generate");
            }
            ImGui::SameLine();
            if (ImGui::Button("Prepare BASS bank")) {
                job("cache.prepare");
            }
            int voices = int(s.bankOptions.voices);
            if (ImGui::SliderInt("Maximum voices", &voices, 1, 256)) {
                s.bankOptions.voices = unsigned(voices);
                s.failBank.reset();
                s.simulating = false;
            }
            if (ImGui::Checkbox("Replace oldest at capacity",
                                &s.bankOptions.stealOldest)) {
                s.failBank.reset();
                s.simulating = false;
            }
            ImGui::TextUnformatted(s.cacheCurrent()
                                       ? "Cache matches current parameters"
                                       : "Cache missing or outdated");
            if (ImGui::CollapsingHeader("Save / load / export")) {
                ImGui::InputText("Analysis file", analysisPath,
                                 sizeof(analysisPath));
                if (ImGui::Button("Save analysis")) {
                    job("song.save", analysisPath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Load analysis")) {
                    job("song.load", fs::path(analysisPath).is_absolute()
                                         ? fs::path(analysisPath)
                                         : s.outputRoot / analysisPath);
                }
                ImGui::InputText("Bank file", cachePath, sizeof(cachePath));
                if (ImGui::Button("Save cache")) {
                    job("cache.save", cachePath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Load cache")) {
                    job("cache.load", fs::path(cachePath).is_absolute()
                                          ? fs::path(cachePath)
                                          : s.outputRoot / cachePath);
                }
                ImGui::InputText("WAV export prefix", cacheExport,
                                 sizeof(cacheExport));
                if (ImGui::Button("Export samples + index")) {
                    job("cache.export", cacheExport);
                }
            }
            const auto *song = s.songAnalysis ? s.songAnalysis.get()
                               : s.failCache  ? &s.failCache->song
                                              : nullptr;
            if (song && !song->regions.empty()) {
                selectedRegion =
                    std::min(selectedRegion, song->regions.size() - 1);
                int selected = int(selectedRegion);
                if (ImGui::SliderInt("Region", &selected, 0,
                                     int(song->regions.size() - 1))) {
                    selectedRegion = size_t(selected);
                }
                const auto &r = song->regions[selectedRegion];
                ImGui::Text("[%0.2f, %0.2f) s | Set %u%s",
                            double(r.begin) / song->info.rate,
                            double(r.end) / song->info.rate, r.setId,
                            r.silent ? " | silence" : "");
                ImGui::Text("Representative %.3f s | change %.3f",
                            double(r.representative) / song->info.rate,
                            r.change);
                if (ImGui::Button("Seek representative")) {
                    attempt([&] {
                        s.seek(double(r.representative) / song->info.rate);
                    });
                }
                ImGui::Text("%.2f s | %zu observations | regions %zu -> %zu",
                            song->info.duration, song->observations.size(),
                            song->detectedRegions, song->regions.size());
                ImGui::Text("Scan %.2f ms | segment %.2f ms", song->scanMs,
                            song->segmentMs);
            }
            if (s.failCache) {
                ImGui::Text("%zu unique sets | %zu samples | PCM %.3f MiB",
                            s.failCache->sets.size(),
                            s.failCache->samples.size(),
                            double(s.failCache->pcmBytes()) / 1048576);
                ImGui::Text("Synth %.2f ms | FX %.2f ms | disk %.3f MiB",
                            s.failCache->synthMs, s.failCache->fxMs,
                            double(s.cacheFileBytes) / 1048576);
                if (!s.failCache->samples.empty()) {
                    const auto &sample = s.failCache->samples.front();
                    ImGui::Text("Content %.4f s | final PCM %.4f s",
                                sample.contentSeconds, sample.pcm.seconds());
                }
            }
            ImGui::Separator();
            ImGui::TextUnformatted("Prepared preview / error simulation");
            const char *names[] = {"G", "R", "Y", "B", "O"};
            for (unsigned fret = 0; fret < 5; ++fret) {
                if (fret) {
                    ImGui::SameLine();
                }
                bool selected = (s.cacheMask & (1u << fret)) != 0;
                if (ImGui::Checkbox(names[fret], &selected)) {
                    s.cacheMask ^= 1u << fret;
                }
            }
            ImGui::Combo("Preview source", &adaptiveMode,
                         "Adaptive cache\0Direct chord at cursor\0Individual "
                         "samples at cursor\0Original generic sounds\0");
            const char *modes[] = {"cache", "direct", "individual", "generic"};
            if (ImGui::Button("Trigger selected frets")) {
                attempt([&] {
                    s.adaptivePreview(modes[adaptiveMode], s.music.position(),
                                      s.cacheMask);
                });
            }
            for (unsigned fret = 0; fret < 5; ++fret) {
                if (fret) {
                    ImGui::SameLine();
                }
                const auto label = std::string("Hit ") + names[fret];
                if (ImGui::Button(label.c_str())) {
                    attempt([&] {
                        s.adaptivePreview("cache", s.music.position(),
                                          1u << fret);
                    });
                }
            }
            if (ImGui::Button(
                    "Prepare direct + individual comparison at cursor")) {
                job("cache.compare");
            }
            if (s.directProduct) {
                ImGui::Text("Comparison fixed at %.3f s, mask %d",
                            s.directProduct->time,
                            s.directProduct->settings.synth.fretMask);
            }
            ImGui::TextWrapped("Individual mixing uses 1/N gain. This "
                               "comparison holds the analysis point constant; "
                               "cache playback follows the music position.");
            ImGui::InputText("Generic sounds directory", genericPath,
                             sizeof(genericPath));
            if (ImGui::Button("Prepare original sounds")) {
                s.genericDirectory = genericPath;
                job("cache.generic");
            }
            ImGui::InputDouble("Error interval (s)", &s.simulationInterval);
            if (ImGui::Button(s.simulating ? "Stop error simulation"
                                           : "Start error simulation")) {
                attempt([&] {
                    if (s.simulating) {
                        s.stopAdaptive();
                        return;
                    }
                    if (!s.failBank || !s.cacheCurrent() ||
                        s.simulationInterval < .01 ||
                        s.simulationInterval > 60 ||
                        !std::isfinite(s.simulationInterval) || !s.cacheMask) {
                        throw audio::Error("Prepare bank and choose valid "
                                           "simulation interval/frets");
                    }
                    s.music.play();
                    s.simulationNext = s.simulationPrevious =
                        s.music.position();
                    s.simulating = true;
                });
            }
            if (ImGui::Button("Stop all fail previews")) {
                attempt([&] { s.stopAdaptive(); });
            }
            if (s.failBank) {
                const auto stats = s.failBank->stats();
                ImGui::Text("Prepare %.3f ms | trigger API %.5f ms",
                            stats.prepareMs, stats.lastTriggerMs);
                ImGui::Text(
                    "Active %u | peak %u | rejected %llu | replaced %llu",
                    stats.active, stats.peakActive,
                    (unsigned long long)stats.rejected,
                    (unsigned long long)stats.replaced);
                ImGui::TextDisabled("API timing is not audible latency. PCM "
                                    "upload adds a BASS copy.");
            }
            if (ImGui::CollapsingHeader("Sustained region experiment")) {
                if (ImGui::Button("Prepare five loops at cursor region")) {
                    job("cache.sustain");
                }
                ImGui::SliderInt("Release fade (ms)", &releaseMs, 0, 1000);
                for (unsigned fret = 0; fret < 5; ++fret) {
                    if (fret) {
                        ImGui::SameLine();
                    }
                    const auto label = std::string("Hold ") + names[fret];
                    if (ImGui::Checkbox(label.c_str(), &held[fret])) {
                        attempt([&] {
                            if (!s.sustained) {
                                held[fret] = false;
                                throw audio::Error(
                                    "Prepare sustained preview first");
                            }
                            if (held[fret]) {
                                s.sustained->press(fret, s.previewVolume / 5);
                            } else {
                                s.sustained->release(fret, unsigned(releaseMs));
                            }
                        });
                    }
                }
            }
            if (ImGui::CollapsingHeader("Sensitivity experiment")) {
                ImGui::SliderInt("Measured triggers", &adaptiveTrials, 1, 1000);
                if (ImGui::Button("Compare four profiles / export JSON")) {
                    job("cache.benchmark", "adaptive-benchmark.json");
                }
                ImGui::TextWrapped(
                    "Runs use identical observations and synthesis settings. "
                    "Evaluate exported banks by listening; more regions are "
                    "not a quality score.");
                if (s.cacheBenchmarkReport &&
                    ImGui::BeginTable("Adaptive profiles", 6,
                                      ImGuiTableFlags_Borders |
                                          ImGuiTableFlags_ScrollX)) {
                    for (const char *label :
                         {"Profile", "Regions / sets", "Disk MiB", "Prepare ms",
                          "Accepted p95 ms", "RSS growth KiB"}) {
                        ImGui::TableSetupColumn(label);
                    }
                    ImGui::TableHeadersRow();
                    for (const auto &c :
                         (*s.cacheBenchmarkReport)["cases"].GetArray()) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%s%s", c["profile"].GetString(),
                                    c["deduplicate"].GetBool() ? " + dedup"
                                                               : "");
                        ImGui::TableNextColumn();
                        ImGui::Text(
                            "%llu / %llu",
                            (unsigned long long)c["regions"].GetUint64(),
                            (unsigned long long)c["sets"].GetUint64());
                        ImGui::TableNextColumn();
                        ImGui::Text("%.3f",
                                    c["file_bytes"].GetUint64() / 1048576.);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.3f", c["prepare_ms"].GetDouble());
                        ImGui::TableNextColumn();
                        if (c.HasMember("accepted_trigger_api")) {
                            ImGui::Text("%.5f",
                                        c["accepted_trigger_api"]["p95_ms"]
                                            .GetDouble());
                        }
                        ImGui::TableNextColumn();
                        const auto &memory = c["playback"]["memory"];
                        if (memory["prepared"]["rss_bytes"].IsNumber() &&
                            memory["loaded"]["rss_bytes"].IsNumber()) {
                            ImGui::Text(
                                "%.1f",
                                (memory["prepared"]["rss_bytes"].GetDouble() -
                                 memory["loaded"]["rss_bytes"].GetDouble()) /
                                    1024);
                        } else {
                            ImGui::TextUnformatted("unavailable");
                        }
                    }
                    ImGui::EndTable();
                    ImGui::TextWrapped(
                        "RSS includes allocator/page effects. Load a bank_path "
                        "from adaptive-benchmark.json to audition that exact "
                        "result.");
                }
            }
        }
        ImGui::End();
    }
    void draw() {
        layout();
        explorer();
        timeline();
        spectrum();
        fx();
        device();
        diagnostics();
        adaptive();
        synth();
        preview();
        if (focusSynth) {
            ImGui::SetWindowFocus("Fail Synth");
            focusSynth = false;
        }
        const std::string error = localError.empty() ? s.error : localError;
        if (!error.empty()) {
            ImGui::SetNextWindowBgAlpha(.97f);
            ImGui::SetNextWindowPos({ImGui::GetMainViewport()->WorkPos.x + 25,
                                     ImGui::GetMainViewport()->WorkPos.y + 55},
                                    ImGuiCond_Appearing);
            if (ImGui::Begin("Operation error", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 600);
                ImGui::TextWrapped("%s", error.c_str());
                ImGui::PopTextWrapPos();
                if (ImGui::Button("Dismiss")) {
                    localError.clear();
                    s.error.clear();
                }
            }
            ImGui::End();
        }
    }
};
} // namespace

int runGui(Session &session, bool rpc, int frames, const std::string &capture) {
    Window window(session.outputRoot);
    UI ui(session);
    glfwSetWindowUserPointer(window.window, &ui);
    glfwSetDropCallback(
        window.window, [](GLFWwindow *w, int count, const char **paths) {
            auto &ui = *static_cast<UI *>(glfwGetWindowUserPointer(w));
            if (count > 0) {
                ui.attempt([&] { ui.s.load(fs::u8path(paths[0])); });
            }
        });
    StdinPump input;
    int frame = 0;
    while (!glfwWindowShouldClose(window.window) && !session.shutdown) {
        glfwPollEvents();
        session.tick();
        if (rpc) {
            input.poll(session);
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ui.draw();
        ImGui::Render();
        int width = 0, height = 0;
        glfwGetFramebufferSize(window.window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(.055f, .065f, .085f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ++frame;
        if (frames > 0 && frame >= frames && !capture.empty()) {
            screenshot(session.artifact(capture), window.window);
        }
        glfwSwapBuffers(window.window);
        if (frames > 0 && frame >= frames) {
            break;
        }
    }
    return 0;
}
} // namespace AudioStudio
