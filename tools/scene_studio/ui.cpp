#include "json.h"
#include "studio.h"
#include <algorithm>
#include <cstdio>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

namespace Studio {
UI::UI(Session &session) : window(session.context.window) {
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
    releaseMouse();
    if (initialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

void UI::releaseMouse() {
    if (flying) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    flying = orbiting = panning = dragged = false;
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
        ImGui::DockBuilderDockWindow("Camera", right);
        ImGui::DockBuilderDockWindow("Visualization", right);
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
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Synthetic")) {
            attempt([&] { s.load("scene", "synthetic"); });
        }
        if (ImGui::Button("Reload")) {
            attempt([&] { s.reload(); });
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset settings")) {
            attempt([&] { s.reset(); });
        }
        static ImGuiTextFilter filter;
        filter.Draw("Filter parts");
        const auto &parts = s.inspection().parts;
        for (size_t i = 0; i < parts.size(); ++i) {
            const auto &p = parts[i];
            const std::string label =
                p.object + " / " + p.group + " / " + p.material;
            if (!filter.PassFilter(label.c_str())) {
                continue;
            }
            ImGui::PushID(int(i));
            bool visible = s.isVisible(i);
            if (ImGui::Checkbox("##visible", &visible)) {
                attempt(
                    [&] { s.visibility(visible ? "show" : "hide", int(i)); });
            }
            ImGui::SameLine();
            if (ImGui::Selectable(label.c_str(), s.selection() == int(i))) {
                s.select(int(i));
            }
            ImGui::PopID();
        }
        if (ImGui::Button("Show all")) {
            attempt([&] { s.visibility("show_all"); });
        }
        if (s.selection() >= 0) {
            ImGui::SameLine();
            if (ImGui::Button("Isolate")) {
                attempt([&] { s.visibility("isolate", s.selection()); });
            }
            if (ImGui::Button("Focus selection (F)")) {
                attempt([&] { s.frame(true); });
            }
            ImGui::SameLine();
            if (ImGui::Button("Hide")) {
                attempt([&] { s.visibility("hide", s.selection()); });
            }
        }
    }
    ImGui::End();
    if (ImGui::Begin("Inspector")) {
        ImGui::TextWrapped("Parts and materials are read-only. OBJ transforms "
                           "are baked into vertices.");
        const auto &inspection = s.inspection();
        const int selected = s.selection();
        size_t vertices = 0, triangles = 0;
        Bounds bounds;
        for (const auto &part : inspection.parts) {
            vertices += part.vertices;
            triangles += part.triangles;
            bounds.include(part.minimum, part.maximum);
        }
        ImGui::Text("%zu parts / %zu materials", inspection.parts.size(),
                    inspection.materials.size());
        ImGui::Text("%zu triangles / %zu expanded vertices", triangles,
                    vertices);
        ImGui::Text("Model size: %.3g / %.3g / %.3g",
                    bounds.maximum[0] - bounds.minimum[0],
                    bounds.maximum[1] - bounds.minimum[1],
                    bounds.maximum[2] - bounds.minimum[2]);
        if (selected >= 0 && size_t(selected) < inspection.parts.size()) {
            const auto &p = inspection.parts[size_t(selected)];
            ImGui::TextWrapped(
                "Object: %s\nGroup: %s\nMaterial: %s\nVertices: %zu",
                p.object.c_str(), p.group.c_str(), p.material.c_str(),
                p.vertices);
            ImGui::Text("ID %zu / %zu triangles", p.id, p.triangles);
            ImGui::Text("Size: %.3g / %.3g / %.3g", p.maximum[0] - p.minimum[0],
                        p.maximum[1] - p.minimum[1],
                        p.maximum[2] - p.minimum[2]);
            auto texture = inspection.textures.find(p.material);
            if (texture != inspection.textures.end()) {
                ImGui::TextWrapped("Texture path: %s",
                                   texture->second.path.c_str());
                ImGui::Text("%s / %d x %d",
                            texture->second.loaded ? "loaded" : "unavailable",
                            texture->second.width, texture->second.height);
            }
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
    if (ImGui::Begin("Camera")) {
        auto next = s.settings;
        int navigation = next.camera.free ? 1 : 0;
        int projection = next.camera.orthographic ? 1 : 0;
        if (ImGui::Combo("Navigation", &navigation, "Orbit\0Free\0")) {
            attempt([&] {
                if (navigation) {
                    CameraMath::projectionMode(next.camera, false);
                }
                next.camera.free = navigation == 1;
                s.apply(next);
                releaseMouse();
            });
        }
        if (ImGui::Combo("Projection", &projection,
                         "Perspective\0Orthographic\0")) {
            attempt([&] {
                CameraMath::projectionMode(next.camera, projection == 1);
                s.apply(next);
                releaseMouse();
            });
        }
        if (ImGui::DragFloat("Speed", &next.camera.speed, .1f, .001f, 1.e6f,
                             "%.3f")) {
            attempt([&] { s.apply(next); });
        }
        if (ImGui::SliderFloat("FOV", &next.camera.fov, 5, 120)) {
            attempt([&] { s.apply(next); });
        }
        const char *views[] = {"front", "back", "left",
                               "right", "top",  "bottom"};
        for (int i = 0; i < 6; ++i) {
            if (i % 3) {
                ImGui::SameLine();
            }
            if (ImGui::Button(views[i])) {
                attempt([&] { s.setView(views[i]); });
            }
        }
        ImGui::TextWrapped(
            "Orbit: left drag. Pan: middle drag. Wheel: zoom toward cursor. "
            "Click: select. Double-click: pivot. F: focus.");
        ImGui::TextWrapped(
            "Free: hold right mouse to look; WASD moves, Q/E down/up, Shift "
            "accelerates. Wheel moves forward/back. Escape releases mouse.");
    }
    ImGui::End();
    if (ImGui::Begin("Visualization")) {
        auto next = s.settings;
        bool changed = ImGui::Combo(
            "Mode", &next.viewMode,
            "Game renderer\0Unlit materials\0Wireframe\0Game + wireframe\0");
        changed |= ImGui::Checkbox("Grid", &next.grid);
        changed |= ImGui::Checkbox("Axes", &next.axes);
        changed |= ImGui::Checkbox("Bounds", &next.bounds);
        if (changed) {
            attempt([&] { s.apply(next); });
        }
        ImGui::TextWrapped(
            "Axes are centered on visible geometry. Dimensions use model "
            "units. Picking ignores texture alpha cutouts.");
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
        ImGui::TextWrapped(
            "AO is baked: hiding parts does not remove their baked occlusion. "
            "Shadows retain the game light volume.");
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
        static bool helpers = false;
        ImGui::Checkbox("Include helpers / selection", &helpers);
        if (ImGui::Button("Capture viewport")) {
            attempt([&] { s.capture(name, size[0], size[1], {}, helpers); });
        }
        ImGui::TextWrapped("Metadata is written next to the PNG. Capturing "
                           "does not resize the interactive viewport.");
    }
    ImGui::End();
    if (!glfwGetWindowAttrib(window, GLFW_FOCUSED) ||
        ImGui::GetIO().WantTextInput || ImGui::IsKeyPressed(ImGuiKey_Escape) ||
        (flying && (!s.settings.camera.free || !ImGui::IsMouseDown(1)))) {
        releaseMouse();
    }
    bool viewportActive = false;
    if (ImGui::Begin("Viewport")) {
        if (ImGui::Button("Frame visible")) {
            attempt([&] { s.frame(); });
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset camera")) {
            attempt([&] { s.resetCamera(); });
        }
        ImGui::SameLine();
        if (ImGui::Button("Focus (F)")) {
            attempt([&] { s.frame(true); });
        }
        ImGui::TextUnformatted(
            s.settings.camera.free
                ? "Right hold: look / WASD / Q,E | Wheel: move"
                : "Left: orbit/select | Middle: pan | Wheel: cursor zoom");
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
                viewportActive = true;
                ImGui::Image(ImTextureID(s.viewport.texture), available,
                             ImVec2(0, 1), ImVec2(1, 0));
                const bool hovered = ImGui::IsItemHovered();
                const auto origin = ImGui::GetItemRectMin();
                const int width = int(available.x), height = int(available.y);
                const float mx = std::clamp(io.MousePos.x - origin.x, 0.f,
                                            float(width) - .01f);
                const float my = std::clamp(io.MousePos.y - origin.y, 0.f,
                                            float(height) - .01f);
                const bool focused =
                    glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
                if (!focused || io.WantTextInput ||
                    ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                    (flying &&
                     (!s.settings.camera.free || !ImGui::IsMouseDown(1)))) {
                    releaseMouse();
                } else {
                    if (hovered && ImGui::IsMouseClicked(0)) {
                        orbiting = true;
                        dragged = false;
                    }
                    if (hovered && ImGui::IsMouseClicked(2)) {
                        panning = true;
                    }
                    if (hovered && s.settings.camera.free &&
                        ImGui::IsMouseClicked(1)) {
                        flying = true;
                        glfwSetInputMode(window, GLFW_CURSOR,
                                         GLFW_CURSOR_DISABLED);
                        glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
                    }
                    if (orbiting && ImGui::IsMouseDragging(0)) {
                        dragged = true;
                    }
                    if (hovered && ImGui::IsMouseDoubleClicked(0)) {
                        attempt([&] {
                            s.pickSelection(mx, my, width, height, true);
                        });
                        orbiting = false;
                    }
                    if (orbiting && !ImGui::IsMouseDown(0)) {
                        if (!dragged && hovered && ImGui::IsMouseReleased(0)) {
                            attempt([&] {
                                s.pickSelection(mx, my, width, height, false);
                            });
                        }
                        orbiting = false;
                    }
                    if (!ImGui::IsMouseDown(2)) {
                        panning = false;
                    }
                    if (hovered && ImGui::IsKeyPressed(ImGuiKey_F)) {
                        attempt([&] { s.frame(true, width, height); });
                    }
                    if (flying) {
                        double x, y;
                        glfwGetCursorPos(window, &x, &y);
                        Vec local{float(ImGui::IsKeyDown(ImGuiKey_D)) -
                                      float(ImGui::IsKeyDown(ImGuiKey_A)),
                                  float(ImGui::IsKeyDown(ImGuiKey_E)) -
                                      float(ImGui::IsKeyDown(ImGuiKey_Q)),
                                  float(ImGui::IsKeyDown(ImGuiKey_W)) -
                                      float(ImGui::IsKeyDown(ImGuiKey_S))};
                        attempt([&] {
                            s.fly(float(x - lastMouseX), float(y - lastMouseY),
                                  local, io.DeltaTime, io.KeyShift);
                        });
                        lastMouseX = x;
                        lastMouseY = y;
                    }
                    const bool orbit =
                        orbiting && dragged && !s.settings.camera.free;
                    if (orbit || panning ||
                        ((hovered || flying) && io.MouseWheel)) {
                        attempt([&] {
                            s.navigate(orbit ? io.MouseDelta.x : 0,
                                       orbit ? io.MouseDelta.y : 0,
                                       panning ? io.MouseDelta.x : 0,
                                       panning ? io.MouseDelta.y : 0,
                                       (hovered || flying) ? io.MouseWheel : 0,
                                       mx, my, width, height);
                        });
                    }
                }
            }
        }
    }
    ImGui::End();
    if (!viewportActive) {
        releaseMouse();
    }
    if (ImGui::Begin("Diagnostics")) {
        for (const auto &warning : s.inspection().warnings) {
            ImGui::TextWrapped("Asset: %s", warning.c_str());
        }
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
