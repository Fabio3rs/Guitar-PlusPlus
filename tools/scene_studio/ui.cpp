#include "json.h"
#include "studio.h"
#include <algorithm>
#include <cstdio>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

namespace Studio {
UI::UI(Session &session) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ini = (session.outputRoot / "layout.ini").string();
    io.IniFilename = ini.c_str();
    ImGui::StyleColorsDark();
    if (!ImGui_ImplGlfw_InitForOpenGL(session.context.window, true)) {
        ImGui::DestroyContext();
        throw Error("imgui", "GLFW backend initialization failed");
    }
    if (!ImGui_ImplOpenGL3_Init("#version 110")) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw Error("imgui", "OpenGL backend initialization failed");
    }
    if (!ImGui_ImplOpenGL3_CreateDeviceObjects()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw Error("imgui", "Cannot compile ImGui GLSL 1.10 shaders");
    }
    initialized = true;
}

UI::~UI() {
    if (initialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

void UI::draw(Session &s) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    const auto dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    static bool first = true;
    if (first && !fs::exists(ini)) {
        ImGui::DockBuilderRemoveNode(dock);
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->WorkSize);
        ImGuiID center = dock,
                left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, .22f,
                                                   nullptr, &center);
        ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right,
                                                    .30f, nullptr, &center);
        ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down,
                                                     .23f, nullptr, &center);
        ImGui::DockBuilderDockWindow("Viewport", center);
        ImGui::DockBuilderDockWindow("Scene Explorer", left);
        ImGui::DockBuilderDockWindow("Inspector", left);
        ImGui::DockBuilderDockWindow("Lighting", right);
        ImGui::DockBuilderDockWindow("Shadows / AO", right);
        ImGui::DockBuilderDockWindow("Capture", right);
        ImGui::DockBuilderDockWindow("Diagnostics", bottom);
        ImGui::DockBuilderFinish(dock);
    }
    first = false;
    auto attempt = [&](auto operation) {
        try {
            operation();
            error.clear();
        } catch (const std::exception &e) {
            error = e.what();
        }
    };
    if (ImGui::Begin("Scene Explorer")) {
        ImGui::Text("Scene: %s", s.sceneLabel().c_str());
        static char file[1024] = "";
        static int type = 0;
        ImGui::Combo("Source type", &type, "Scene ID\0Lua file\0OBJ model\0");
        ImGui::InputText("Source", file, sizeof(file));
        if (ImGui::Button("Load")) {
            attempt([&] {
                s.load(type == 0   ? "scene"
                       : type == 1 ? "file"
                                   : "model",
                       file);
                selected = -1;
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Synthetic")) {
            attempt([&] {
                s.load("scene", "synthetic");
                selected = -1;
            });
        }
        if (ImGui::Button("Reload")) {
            attempt([&] {
                s.reload();
                selected = -1;
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset settings")) {
            attempt([&] { s.reset(); });
        }
        const auto &parts = s.inspection().parts;
        for (size_t i = 0; i < parts.size(); ++i) {
            const auto &p = parts[i];
            ImGui::PushID(int(i));
            if (ImGui::Selectable(
                    (p.object + " / " + p.group + " / " + p.material).c_str(),
                    selected == int(i))) {
                selected = int(i);
            }
            ImGui::PopID();
        }
    }
    ImGui::End();
    if (ImGui::Begin("Inspector")) {
        ImGui::TextWrapped("Parts and materials are read-only. OBJ transforms "
                           "are baked into vertices.");
        const auto &inspection = s.inspection();
        if (selected >= 0 && size_t(selected) < inspection.parts.size()) {
            const auto &p = inspection.parts[size_t(selected)];
            ImGui::TextWrapped(
                "Object: %s\nGroup: %s\nMaterial: %s\nVertices: %zu",
                p.object.c_str(), p.group.c_str(), p.material.c_str(),
                p.vertices);
            auto m = inspection.materials.find(p.material);
            if (m != inspection.materials.end()) {
                const auto &v = m->second;
                ImGui::Text("Diffuse %.3f %.3f %.3f", v.diffuse[0],
                            v.diffuse[1], v.diffuse[2]);
                ImGui::Text("Specular %.3f %.3f %.3f", v.specular[0],
                            v.specular[1], v.specular[2]);
                ImGui::Text("Shininess %.2f / opacity %.2f", v.shininess,
                            v.opacity);
                ImGui::TextWrapped("Texture: %s", v.diffuseMap.c_str());
            }
        }
    }
    ImGui::End();
    if (ImGui::Begin("Lighting")) {
        Settings next = s.settings;
        if (ImGui::DragFloat3("Light position", next.light.data(), .03f)) {
            attempt([&] { s.apply(next); });
        }
        ImGui::TextWrapped("Uses Guitar++ scene lighting and materials. The "
                           "current scene pipeline is static.");
    }
    ImGui::End();
    if (ImGui::Begin("Shadows / AO")) {
        Settings next = s.settings;
        bool changed =
            ImGui::Combo("Quality", &next.quality,
                         "Original\0Lighting\0Shadows 512\0Shadows 1024\0");
        changed |= ImGui::Checkbox("Ambient occlusion", &next.ao);
        float bias = float(next.bias);
        if (ImGui::SliderFloat("Shadow bias", &bias, 0, .02f, "%.5f")) {
            next.bias = bias;
            changed = true;
        }
        if (changed) {
            attempt([&] { s.apply(next); });
        }
        const auto d = CBackgroundRenderer::inst().diagnostics();
        ImGui::Text("Shadow support: %s", d.shadowSupported ? "yes" : "no");
        ImGui::Text("Shadow updates: %zu",
                    CBackgroundRenderer::inst().shadowUpdates());
        ImGui::Text("AO atlas: %s",
                    s.inspection().aoAtlas ? "available" : "unavailable");
        ImGui::Text("Shadow allocation: %s", d.shadowFailed ? "failed" : "OK");
    }
    ImGui::End();
    if (ImGui::Begin("Capture")) {
        static int size[2] = {960, 540};
        static char name[512] = "viewport.png";
        ImGui::InputInt2("Resolution", size);
        ImGui::InputText("PNG filename", name, sizeof(name));
        ImGui::TextWrapped("Output directory: %s",
                           s.outputRoot.string().c_str());
        if (ImGui::Button("Capture clean viewport")) {
            attempt([&] { s.capture(name, size[0], size[1]); });
        }
        ImGui::TextWrapped("Metadata is written next to the PNG. Capturing "
                           "does not resize the interactive viewport.");
    }
    ImGui::End();
    if (ImGui::Begin("Viewport")) {
        if (ImGui::Button("Frame object")) {
            attempt([&] { s.frame(); });
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset camera")) {
            attempt([&] {
                auto old = s.settings;
                s.reset();
                auto next = s.settings;
                next.light = old.light;
                next.quality = old.quality;
                next.ao = old.ao;
                next.bias = old.bias;
                s.apply(next);
            });
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Left: orbit | Middle: pan | Wheel: zoom");
        ImVec2 available = ImGui::GetContentRegionAvail();
        if (available.x >= 1 && available.y >= 1) {
            const auto &io = ImGui::GetIO();
            try {
                s.render(s.viewport,
                         int(available.x * io.DisplayFramebufferScale.x),
                         int(available.y * io.DisplayFramebufferScale.y));
            } catch (const std::exception &e) {
                error = e.what();
            }
            if (s.viewport.texture) {
                ImGui::Image(ImTextureID(s.viewport.texture), available,
                             ImVec2(0, 1), ImVec2(1, 0));
                const bool hovered = ImGui::IsItemHovered();
                static bool orbit = false, pan = false;
                if (hovered && ImGui::IsMouseClicked(0)) {
                    orbit = true;
                }
                if (hovered && ImGui::IsMouseClicked(2)) {
                    pan = true;
                }
                if (!ImGui::IsMouseDown(0)) {
                    orbit = false;
                }
                if (!ImGui::IsMouseDown(2)) {
                    pan = false;
                }
                if (orbit || pan || (hovered && io.MouseWheel)) {
                    attempt([&] {
                        s.navigate(orbit ? io.MouseDelta.x : 0,
                                   orbit ? io.MouseDelta.y : 0,
                                   pan ? io.MouseDelta.x : 0,
                                   pan ? io.MouseDelta.y : 0,
                                   hovered ? io.MouseWheel : 0);
                    });
                }
            }
        }
    }
    ImGui::End();
    if (ImGui::Begin("Diagnostics")) {
        const auto &c = s.context.caps;
        ImGui::TextWrapped("%s | %s | %s | %s (%s)", c.vendor.c_str(),
                           c.renderer.c_str(), c.version.c_str(),
                           c.backend.c_str(), c.acceleration.c_str());
        ImGui::Text("Viewport %d x %d | CPU submission %.3f ms | UI %.1f FPS",
                    s.viewport.width, s.viewport.height, s.lastFrameMs,
                    ImGui::GetIO().Framerate);
        ImGui::Text("FBO %d | NPOT %d | depth textures %d | max texture %d",
                    c.fbo, c.npot, c.depthTexture, c.maxTexture);
        if (!error.empty()) {
            ImGui::TextWrapped("Error: %s", error.c_str());
        }
        if (ImGui::TreeNode("Scene and effective renderer state")) {
            const auto state = s.state();
            ImGui::TextWrapped("%s", state.c_str());
            ImGui::TreePop();
        }
    }
    ImGui::End();
    ImGui::Render();
    int w = 0, h = 0;
    glfwGetFramebufferSize(s.context.window, &w, &h);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glViewport(0, 0, w, h);
    glClearColor(.07f, .08f, .1f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
} // namespace Studio
