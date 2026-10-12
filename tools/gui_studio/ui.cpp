#include "ui.h"
#include <algorithm>
#include <cmath>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>
#include <stdexcept>

namespace GuiStudio {
namespace {
bool number(const char *label, double &v, float speed = .005f) {
    return ImGui::DragScalar(label, ImGuiDataType_Double, &v, speed, nullptr,
                             nullptr, "%.4f");
}

bool color(const char *label, ui::Color &c) {
    float rgba[]{float(c.r), float(c.g), float(c.b), float(c.a)};
    if (!ImGui::ColorEdit4(label, rgba)) {
        return false;
    }
    c = {rgba[0], rgba[1], rgba[2], rgba[3]};
    return true;
}

bool optionalNumber(const char *label, std::optional<double> &value,
                    double fallback) {
    ImGui::PushID(label);
    bool enabled = value.has_value();
    bool changed = ImGui::Checkbox("Override", &enabled);
    ImGui::SameLine();
    if (changed) {
        value = enabled ? std::optional<double>(fallback) : std::nullopt;
    }
    double current = value.value_or(fallback);
    ImGui::BeginDisabled(!enabled);
    if (number(label, current)) {
        value = current;
        changed = true;
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    return changed;
}

bool optionalColor(const char *label, std::optional<ui::Color> &value,
                   ui::Color fallback) {
    ImGui::PushID(label);
    bool enabled = value.has_value();
    bool changed = ImGui::Checkbox("Override", &enabled);
    ImGui::SameLine();
    if (changed) {
        value = enabled ? std::optional<ui::Color>(fallback) : std::nullopt;
    }
    auto current = value.value_or(fallback);
    ImGui::BeginDisabled(!enabled);
    if (color(label, current)) {
        value = current;
        changed = true;
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    return changed;
}

bool rectangle(const char *label, CMenu::Rect &r) {
    bool changed = false;
    if (ImGui::TreeNode(label)) {
        changed |= number("Left", r.left);
        changed |= number("Bottom", r.bottom);
        changed |= number("Right", r.right);
        changed |= number("Top", r.top);
        ImGui::TreePop();
    }
    return changed;
}

bool strings(const char *label, std::vector<std::string> &list) {
    bool changed = false;
    if (ImGui::TreeNode(label)) {
        for (size_t i = 0; i < list.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            changed |= ImGui::InputText("##value", &list[i]);
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) {
                list.erase(list.begin() + static_cast<ptrdiff_t>(i));
                changed = true;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        if (ImGui::Button("Add entry")) {
            list.emplace_back();
            changed = true;
        }
        ImGui::TreePop();
    }
    return changed;
}

void drawPreview(const ImDrawList *, const ImDrawCmd *command) {
    const auto &view = *static_cast<PreviewDraw *>(command->UserCallbackData);
    auto &s = *view.session;
    const auto &io = ImGui::GetIO();
    const int w = std::max(1, int(view.size.x * io.DisplayFramebufferScale.x));
    const int h = std::max(1, int(view.size.y * io.DisplayFramebufferScale.y));
    const int x = int(view.position.x * io.DisplayFramebufferScale.x);
    const int y = int((io.DisplaySize.y - view.position.y - view.size.y) *
                      io.DisplayFramebufferScale.y);
    GLint program = 0, mode = 0, array = 0, element = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_MATRIX_MODE, &mode);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    CEngine::engine().invalidateRenderBindings();
    glUseProgram(0);
    GLint attributes = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attributes);
    std::vector<GLint> enabled(static_cast<size_t>(attributes));
    for (GLint i = 0; i < attributes; ++i) {
        glGetVertexAttribiv(static_cast<GLuint>(i),
                            GL_VERTEX_ATTRIB_ARRAY_ENABLED,
                            &enabled[static_cast<size_t>(i)]);
        glDisableVertexAttribArray(static_cast<GLuint>(i));
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glViewport(x, y, w, h);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, w, h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    const double aspect = double(w) / h;
    glOrtho(-aspect, aspect, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glColor4d(1, 1, 1, 1);
    if (view.font) {
        const auto metrics =
            s.fonts.empty()
                ? CFonts::FaceMetrics{}
                : s.fonts[static_cast<size_t>(s.selectedFont)].metrics;
        // Use logical pixels so resizing the panel does not shrink the text.
        const double scale = 2.0 * view.fontPixels / view.size.y;
        const double originY =
            -(metrics.baseline + (metrics.ascent - metrics.descent) / 2) *
            scale;
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_LINES);
        for (double line : {metrics.baseline, metrics.baseline + metrics.ascent,
                            metrics.baseline - metrics.descent}) {
            glColor4d(.2, .65, .7, 1);
            glVertex2d(-aspect, originY + line * scale);
            glVertex2d(aspect, originY + line * scale);
        }
        glEnd();
        glEnable(GL_TEXTURE_2D);
        glColor4d(1, 1, 1, 1);
        s.registry().drawTextInScreen(view.text, -aspect + 16.0 / view.size.y,
                                      originY, scale, s.context.font);
    } else {
        s.render();
    }
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(static_cast<GLenum>(mode));
    glPopClientAttrib();
    glPopAttrib();
    for (GLint i = 0; i < attributes; ++i) {
        if (enabled[static_cast<size_t>(i)]) {
            glEnableVertexAttribArray(static_cast<GLuint>(i));
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(array));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(element));
    glUseProgram(static_cast<GLuint>(program));
    CEngine::engine().invalidateRenderBindings();
}

void previewItem(PreviewDraw &draw, ImVec2 size) {
    draw.position = ImGui::GetCursorScreenPos();
    draw.size = size;
    ImGui::InvisibleButton(draw.font ? "font-preview" : "menu-preview", size);
    auto *list = ImGui::GetWindowDrawList();
    list->AddRectFilled(draw.position,
                        {draw.position.x + size.x, draw.position.y + size.y},
                        IM_COL32(20, 23, 29, 255));
    list->AddCallback(drawPreview, &draw);
    list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}
} // namespace

UI::UI(Session &s, GLFWwindow *w) : session(s), window(w) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ini = (s.output / "layout.ini").string();
    ImGui::GetIO().IniFilename = ini.c_str();
    ImGui::StyleColorsDark();
    ImFontConfig config;
    config.SizePixels = 15;
    ImGui::GetIO().Fonts->AddFontDefault(&config);
    glfwSetWindowUserPointer(w, this);
    glfwSetKeyCallback(w, [](GLFWwindow *p, int key, int, int action,
                             int mods) {
        static_cast<UI *>(glfwGetWindowUserPointer(p))->key(key, action, mods);
    });
    glfwSetCharCallback(w, [](GLFWwindow *p, unsigned code) {
        static_cast<UI *>(glfwGetWindowUserPointer(p))->character(code);
    });
    glfwSetWindowFocusCallback(w, [](GLFWwindow *p, int focused) {
        static_cast<UI *>(glfwGetWindowUserPointer(p))->focus(focused != 0);
    });
    if (!ImGui_ImplGlfw_InitForOpenGL(w, true) ||
        !ImGui_ImplOpenGL3_Init("#version 110")) {
        throw std::runtime_error("Cannot initialize Dear ImGui");
    }
    menuDraw.session = fontDraw.session = &s;
    fontDraw.font = true;
    fontFile = s.fontSource;
    menuFile = s.menuSource;
}

UI::~UI() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwSetKeyCallback(window, nullptr);
    glfwSetCharCallback(window, nullptr);
    glfwSetWindowFocusCallback(window, nullptr);
    glfwSetWindowUserPointer(window, nullptr);
}

void UI::attempt(std::function<void()> operation) {
    try {
        session.error.clear();
        operation();
    } catch (const std::exception &e) {
        session.error = e.what();
    }
}

void UI::protect(std::function<void()> operation) {
    if (session.dirtyFonts || session.dirtyMenus) {
        pending = std::move(operation);
    } else {
        attempt(std::move(operation));
    }
}

void UI::requestClose() {
    protect([this] { closeApproved = true; });
}

void UI::key(int keyCode, int action, int mods) {
    if (previewFocus && session.testing) {
        CControls::MenuEvent event;
        event.key = keyCode;
        event.action = action;
        event.mods = mods;
        session.context.events.push_back(event);
    }
}

void UI::character(unsigned code) {
    if (previewFocus && session.testing) {
        CControls::MenuEvent event;
        event.kind = CControls::MenuEvent::Character;
        event.codepoint = code;
        session.context.events.push_back(event);
    }
}

void UI::focus(bool focused) {
    if (!focused) {
        CControls::MenuEvent event;
        event.kind = CControls::MenuEvent::Focus;
        session.context.events.push_back(event);
        previewFocus = mouseCaptured = false;
        atlasKeyboardFocus = false;
    }
}

void UI::menus() {
    auto &s = session;
    ImGui::PushItemWidth(
        std::max(100.f, ImGui::GetContentRegionAvail().x - 180));
    ImGui::InputText("Open Lua", &menuFile);
    ImGui::SameLine();
    if (ImGui::Button("Open##menus")) {
        protect([this] { session.loadMenus(menuFile); });
    }
    ImGui::InputText("Export Lua", &exportMenu);
    ImGui::SameLine();
    if (ImGui::Button("Export##menus")) {
        attempt([&] { s.saveMenus(s.output / exportMenu); });
    }
    if (ImGui::Button("New menu")) {
        ui::MenuDefinition menu;
        menu.id = "menu-" + std::to_string(s.menus.size() + 1);
        while (std::any_of(s.menus.begin(), s.menus.end(),
                           [&](const auto &m) { return m.id == menu.id; })) {
            menu.id += "-new";
        }
        s.menus.push_back(menu);
        s.selectedMenu = int(s.menus.size()) - 1;
        s.dirtyMenus = true;
        attempt([&] { s.rebuild(); });
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate menu")) {
        auto menu = s.menus.at(size_t(s.selectedMenu));
        menu.id += "-copy";
        while (std::any_of(s.menus.begin(), s.menus.end(),
                           [&](const auto &m) { return m.id == menu.id; })) {
            menu.id += "-copy";
        }
        s.menus.push_back(menu);
        s.selectedMenu = int(s.menus.size()) - 1;
        s.dirtyMenus = true;
        attempt([&] { s.rebuild(); });
    }
    if (ImGui::BeginCombo("Menu",
                          s.menus.at(size_t(s.selectedMenu)).id.c_str())) {
        for (size_t i = 0; i < s.menus.size(); ++i) {
            if (ImGui::Selectable(s.menus[i].id.c_str(),
                                  s.selectedMenu == int(i))) {
                s.selectedMenu = int(i);
                s.selectedOption = 0;
                attempt([&] { s.rebuild(); });
            }
        }
        ImGui::EndCombo();
    }
    auto next = s.menus.at(size_t(s.selectedMenu));
    bool changed = false;
    ImGui::BeginDisabled(s.testing);
    if (ImGui::CollapsingHeader("Menu properties")) {
        changed |= ImGui::InputText("ID", &next.id);
        changed |= ImGui::InputText("Title", &next.title);
        changed |= ImGui::InputText("Panel texture", &next.background);
        changed |= ImGui::Checkbox("Modal", &next.modal);
        changed |= number("Menu X", next.x);
        changed |= number("Menu Y", next.y);
        int flow = static_cast<int>(next.flow);
        if (ImGui::Combo("Flow", &flow, "Manual\0Vertical\0Horizontal\0")) {
            next.flow = static_cast<CMenu::Flow>(flow);
            changed = true;
        }
        changed |= number("Flow X", next.flowX);
        changed |= number("Flow Y", next.flowY);
        changed |= ImGui::InputInt("Flow group", &next.flowGroup);
        changed |= ImGui::Checkbox("Anchor left", &next.anchorLeft);
        changed |= number("Left margin", next.leftMargin);
        changed |=
            ImGui::Checkbox("Clip / scroll viewport", &next.viewportEnabled);
        changed |= rectangle("Viewport", next.viewport);
        changed |= rectangle("Panel bounds", next.panel);
        if (ImGui::TreeNode("Menu style")) {
            changed |= optionalNumber("Text size", next.style.textSize, .075);
            changed |= optionalNumber("Row step", next.style.rowStep, .1);
            changed |= optionalNumber("Padding", next.style.padding, .08);
            changed |= optionalNumber("Pick gap", next.style.pickGap, .2);
            changed |= optionalColor("Text color", next.style.textColor, {});
            changed |=
                optionalColor("Accent", next.style.accent, {1, .88, .5, 1});
            changed |= optionalColor("Selection", next.style.selection,
                                     {.04, .04, .055, .72});
            changed |= optionalColor("Field", next.style.field,
                                     {.025, .025, .035, .82});
            changed |= optionalColor("Control", next.style.control,
                                     {.15, .17, .21, .9});
            changed |=
                optionalColor("Panel", next.style.panel, {.07, .08, .10, .95});
            int animate = next.style.animateSelection
                              ? (*next.style.animateSelection ? 2 : 1)
                              : 0;
            if (ImGui::Combo("Selection animation", &animate,
                             "Inherited\0Off\0On\0")) {
                next.style.animateSelection =
                    animate == 0 ? std::nullopt
                                 : std::optional<bool>(animate == 2);
                changed = true;
            }
            ImGui::TreePop();
        }
    }
    ImGui::SeparatorText("Controls");
    for (size_t i = 0; i < next.options.size(); ++i) {
        if (ImGui::Selectable(next.options[i].id.c_str(),
                              s.selectedOption == int(i))) {
            s.selectedOption = int(i);
        }
    }
    if (ImGui::Button("Add control")) {
        ui::OptionDefinition option;
        option.id = "control-" + std::to_string(next.options.size() + 1);
        while (std::any_of(next.options.begin(), next.options.end(),
                           [&](const auto &o) { return o.id == option.id; })) {
            option.id += "-new";
        }
        option.text = "Button";
        next.options.push_back(option);
        s.selectedOption = int(next.options.size()) - 1;
        changed = true;
    }
    if (s.selectedOption >= 0 &&
        size_t(s.selectedOption) < next.options.size()) {
        size_t index = size_t(s.selectedOption);
        ImGui::SameLine();
        if (ImGui::Button("Remove control")) {
            next.options.erase(next.options.begin() + s.selectedOption);
            s.selectedOption = -1;
            changed = true;
        } else {
            if (ImGui::Button("Move up") && index > 0) {
                std::swap(next.options[index], next.options[index - 1]);
                --index;
                --s.selectedOption;
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Move down") && index + 1 < next.options.size()) {
                std::swap(next.options[index], next.options[index + 1]);
                ++index;
                ++s.selectedOption;
                changed = true;
            }
            auto &o = next.options[index];
            changed |= ImGui::InputText("Control ID", &o.id);
            int type = static_cast<int>(o.type);
            if (ImGui::Combo(
                    "Type", &type,
                    "OK\0Text input\0Cancel\0Text button\0Multi button\0List "
                    "slider\0UI button\0Drag bar\0Static text\0")) {
                o.type = static_cast<CMenu::menusOPT>(type);
                changed = true;
            }
            changed |= ImGui::InputText("Text", &o.text);
            changed |= ImGui::InputText("Initial value", &o.value);
            changed |= ImGui::InputText("Translation key", &o.languageKey);
            changed |= ImGui::InputText("Texture", &o.texture);
            changed |= number("X", o.x);
            changed |= number("Y", o.y);
            changed |= ImGui::InputInt("Group", &o.group);
            changed |= ImGui::Checkbox("Manual position", &o.manual);
            changed |= ImGui::Checkbox("Anchor right", &o.anchorRight);
            changed |= optionalNumber("Center X", o.centerX, 0);
            changed |= number("Field width", o.fieldWidth);
            changed |= number("List / bar width", o.listWidth);
            changed |= ImGui::Checkbox("Disabled", &o.disabled);
            changed |= ImGui::Checkbox("Back / close", &o.back);
            changed |= ImGui::Checkbox("Enable activation", &o.enableEnter);
            changed |= ImGui::InputInt("Shortcut (GLFW key)", &o.shortcut);
            int bytes = static_cast<int>(o.maxBytes);
            if (ImGui::InputInt("Text byte limit", &bytes)) {
                o.maxBytes = size_t(std::max(0, bytes));
                changed = true;
            }
            changed |= optionalNumber("Control text size", o.style.textSize,
                                      next.style.textSize.value_or(.075));
            changed |=
                optionalColor("Control text color", o.style.textColor,
                              next.style.textColor.value_or(ui::Color{}));
            changed |= strings("List entries", o.items);
            changed |= strings("Actions", o.actions);
            ImGui::TextWrapped("Actions: menu:ID, modal:ID, or an external "
                               "game action identifier.");
        }
    }
    ImGui::EndDisabled();
    ImGui::PopItemWidth();
    if (changed) {
        attempt([&] {
            auto definitions = s.menus;
            definitions.at(size_t(s.selectedMenu)) = next;
            ui::validate(definitions);
            s.menus = std::move(definitions);
            s.dirtyMenus = true;
            s.rebuild();
        });
    }
}

void UI::canvas() {
    auto &s = session;
    if (ImGui::Checkbox("Test interaction", &s.testing)) {
        focus(false);
        attempt([&] { s.rebuild(); });
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart preview")) {
        attempt([&] { s.rebuild(); });
    }
    ImGui::TextWrapped(
        s.testing ? "Click the preview to focus. Escape closes dialogs. Game "
                    "actions are logged."
                  : "Click a control to select it; drag to position it.");
    auto size = ImGui::GetContentRegionAvail();
    size.x = std::max(100.f, size.x);
    size.y = std::max(100.f, size.y);
    s.context.aspect = double(size.x) / size.y;
    previewItem(menuDraw, size);
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsMouseClicked(0)) {
        if (!hovered && previewFocus) {
            focus(false);
        }
        previewFocus = hovered;
        mouseCaptured = hovered;
    }
    const auto mouse = ImGui::GetMousePos();
    s.context.mouseX =
        (double(mouse.x - menuDraw.position.x) / size.x * 2 - 1) *
        s.context.aspect;
    s.context.mouseY = 1 - double(mouse.y - menuDraw.position.y) / size.y * 2;
    if (s.testing) {
        for (int action : {GLFW_PRESS, GLFW_RELEASE}) {
            if ((action == GLFW_PRESS
                     ? hovered && ImGui::IsMouseClicked(0)
                     : mouseCaptured && ImGui::IsMouseReleased(0))) {
                CControls::MenuEvent event;
                event.kind = CControls::MenuEvent::Mouse;
                event.key = GLFW_MOUSE_BUTTON_LEFT;
                event.action = action;
                event.x = s.context.mouseX;
                event.y = s.context.mouseY;
                s.context.events.push_back(event);
            }
        }
        if (hovered && ImGui::GetIO().MouseWheel != 0) {
            CControls::MenuEvent event;
            event.kind = CControls::MenuEvent::Scroll;
            event.y = ImGui::GetIO().MouseWheel;
            s.context.events.push_back(event);
        }
        attempt([&] { s.update(); });
    } else if (auto *menu = s.preview()) {
        if (hovered && ImGui::IsMouseClicked(0)) {
            for (size_t i = 0; i < menu->options.size(); ++i) {
                if (menu->optionRect(i).contains(s.context.mouseX,
                                                 s.context.mouseY)) {
                    s.selectedOption = int(i);
                }
            }
        }
        if (mouseCaptured && ImGui::IsMouseDragging(0) &&
            s.selectedOption >= 0 &&
            size_t(s.selectedOption) < menu->options.size()) {
            menu->beginManualEdit(size_t(s.selectedOption));
            auto &o = menu->options[size_t(s.selectedOption)];
            o.x += ImGui::GetIO().MouseDelta.x * 2 / size.y;
            o.y -= ImGui::GetIO().MouseDelta.y * 2 / size.y;
            auto &definition = s.menus[size_t(s.selectedMenu)];
            definition.flow = menu->flow;
            for (size_t i = 0; i < definition.options.size(); ++i) {
                auto &target = definition.options[i];
                const auto &source = menu->options[i];
                target.x = source.x;
                target.y = source.y;
                target.manual = source.manualPosition;
                target.centerX = source.centerX;
                target.anchorRight = source.anchorRight;
            }
            s.dirtyMenus = true;
        }
        if (s.selectedOption >= 0 &&
            size_t(s.selectedOption) < menu->options.size()) {
            const auto r = menu->optionRect(size_t(s.selectedOption));
            auto screen = [&](double x, double y) {
                return ImVec2{
                    menuDraw.position.x +
                        float((x / s.context.aspect + 1) * size.x / 2),
                    menuDraw.position.y + float((1 - y) * size.y / 2)};
            };
            ImGui::GetWindowDrawList()->AddRect(screen(r.left, r.top),
                                                screen(r.right, r.bottom),
                                                IM_COL32(255, 190, 60, 255));
        }
    }
    if (ImGui::IsMouseReleased(0)) {
        mouseCaptured = false;
    }
}

void UI::fonts() {
    auto &s = session;
    ImGui::InputText("Open font Lua", &fontFile);
    ImGui::SameLine();
    if (ImGui::Button("Open##fonts")) {
        protect([this] { session.loadFonts(fontFile); });
    }
    ImGui::InputText("Export font Lua", &exportFont);
    ImGui::SameLine();
    if (ImGui::Button("Export##fonts")) {
        attempt([&] { s.saveFonts(s.output / exportFont); });
    }
    if (ImGui::Button("New font")) {
        addAtlas = false;
        ImGui::OpenPopup("Atlas setup");
    }
    ImGui::SameLine();
    if (ImGui::Button("Add atlas") && !s.fonts.empty()) {
        addAtlas = true;
        ImGui::OpenPopup("Atlas setup");
    }
    if (ImGui::BeginPopupModal("Atlas setup", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!addAtlas) {
            ImGui::InputText("Font name", &newName);
        }
        ImGui::InputText("TGA file", &newFile);
        ImGui::TextWrapped("Relative paths use the asset root. Rows in the "
                           "mapping are separated by newlines.");
        ImGui::InputInt("Columns", &newColumns);
        ImGui::InputInt("Rows", &newRows);
        ImGui::InputTextMultiline("Codepoint mapping", &newMapping, {420, 150});
        if (ImGui::Button("Create")) {
            attempt([&] {
                auto next = s.fonts;
                if (!addAtlas) {
                    if (std::any_of(
                            next.begin(), next.end(),
                            [&](const auto &f) { return f.name == newName; })) {
                        throw std::runtime_error("Font name already exists");
                    }
                    next.push_back({});
                    next.back().name = newName;
                }
                const size_t target =
                    addAtlas ? size_t(s.selectedFont) : next.size() - 1;
                auto &font = next[target];
                fs::path file(newFile);
                font.atlases.push_back({file.parent_path().string(),
                                        file.filename().string(),
                                        newMapping,
                                        {newColumns, newRows}});
                s.applyFonts(std::move(next));
                s.setFont(target);
                ImGui::CloseCurrentPopup();
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        if (!s.error.empty()) {
            ImGui::TextWrapped("%s", s.error.c_str());
        }
        ImGui::EndPopup();
    }
    if (s.fonts.empty()) {
        ImGui::TextWrapped("No font loaded. Open a font script or create a "
                           "font from a TGA atlas.");
        return;
    }
    if (ImGui::BeginCombo("Font",
                          s.fonts[size_t(s.selectedFont)].name.c_str())) {
        for (size_t i = 0; i < s.fonts.size(); ++i) {
            if (ImGui::Selectable(s.fonts[i].name.c_str(),
                                  size_t(s.selectedFont) == i)) {
                attempt([&] { s.setFont(i); });
            }
        }
        ImGui::EndCombo();
    }
    auto &font = s.fonts[size_t(s.selectedFont)];
    if (ImGui::BeginCombo("Atlas",
                          font.atlases[size_t(s.selectedAtlas)].file.c_str())) {
        for (size_t i = 0; i < font.atlases.size(); ++i) {
            if (ImGui::Selectable(font.atlases[i].file.c_str(),
                                  size_t(s.selectedAtlas) == i)) {
                s.selectedAtlas = int(i);
                pan = {0, 0};
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::CollapsingHeader("Atlas configuration")) {
        // Draft persists while editing; Apply is explicit because rescanning
        // the atlas resets glyph measurements and uploads a replacement.
        static AtlasDefinition draft;
        static std::string source;
        const auto key = font.name + ":" + std::to_string(s.selectedAtlas) +
                         ":" + std::to_string(s.fontGeneration);
        if (source != key) {
            draft = font.atlases[size_t(s.selectedAtlas)];
            source = key;
        }
        ImGui::InputText("Directory", &draft.path);
        ImGui::InputText("File", &draft.file);
        ImGui::InputInt("Grid columns", &draft.grid.columns);
        ImGui::InputInt("Grid rows", &draft.grid.rows);
        ImGui::InputTextMultiline("Mapping", &draft.mapping, {-1, 110});
        if (ImGui::Button("Apply atlas configuration")) {
            attempt([&] {
                auto next = s.fonts;
                next[size_t(s.selectedFont)].atlases[size_t(s.selectedAtlas)] =
                    draft;
                s.applyFonts(std::move(next));
                s.rebuild();
            });
            return;
        }
    }
    auto metrics = font.metrics;
    if (ImGui::CollapsingHeader("Face metrics")) {
        bool changed = number("Baseline", metrics.baseline);
        changed |= number("Ascent", metrics.ascent);
        changed |= number("Descent", metrics.descent);
        if (changed) {
            attempt([&] { s.applyMetrics(metrics); });
        }
    }
    if (inspectedGeneration != s.fontGeneration ||
        inspectedRevision != s.registry().revision() ||
        inspectedFont != font.name) {
        inspection = s.registry().inspect(font.name);
        inspectedGeneration = s.fontGeneration;
        inspectedRevision = s.registry().revision();
        inspectedFont = font.name;
    }
    ImGui::SeparatorText("Codepoints");
    ImGui::BeginChild("glyphs", {0, 150}, ImGuiChildFlags_Borders);
    for (const auto &entry : inspection.glyphs) {
        if (entry.second.texture !=
            s.atlasTextures[size_t(s.selectedFont)][size_t(s.selectedAtlas)]
                ->id) {
            continue;
        }
        char code[32];
        std::snprintf(code, sizeof(code), "U+%04X ", entry.first);
        const auto label =
            std::string(code) + GuiStudio::character(entry.first);
        if (ImGui::Selectable(label.c_str(), s.selectedGlyph == entry.first)) {
            s.selectedGlyph = entry.first;
        }
    }
    ImGui::EndChild();
    const auto found = inspection.glyphs.find(s.selectedGlyph);
    if (found != inspection.glyphs.end()) {
        auto glyph = found->second;
        ImGui::Text("Selected: U+%04X", s.selectedGlyph);
        bool changed = number("Vertical offset (+ up)", glyph.offset);
        changed |= number("Height", glyph.height);
        changed |= number("Advance", glyph.advance);
        changed |= number("Bearing", glyph.bearing);
        if (changed) {
            attempt([&] { s.applyGlyph(s.selectedGlyph, glyph); });
        }
        if (ImGui::Button("Reset glyph metrics")) {
            glyph.height = 1;
            glyph.offset = 0;
            glyph.advance = glyph.visible
                                ? std::max(.3, glyph.right - glyph.left) + .1
                                : 2.0 / 3.0;
            glyph.bearing = glyph.visible ? .05 - glyph.left : 0;
            attempt([&] { s.applyGlyph(s.selectedGlyph, glyph); });
        }
    }
}

void UI::atlas() {
    auto &s = session;
    ImGui::Checkbox("Grid", &grid);
    ImGui::SameLine();
    ImGui::Checkbox("Detected ink bounds", &bounds);
    ImGui::SliderFloat("Zoom", &zoom, .05f, 5.f, "%.2fx");
    ImGui::SameLine();
    if (ImGui::Button("Fit atlas")) {
        fitAtlas = true;
    }
    ImGui::TextUnformatted(
        "Click atlas, then type to select a glyph (Shift supported).");
    const auto &io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        atlasKeyboardFocus = false;
    }
    // Consume composed Unicode characters, not physical key codes. ImGui's
    // queue already follows the OS keyboard layout and Shift/dead keys.
    // AltGr may be reported as Ctrl+Alt. Allow its generated characters, but
    // leave Ctrl-only, Alt-only and Super shortcuts to the editor/OS.
    const bool shortcutModifier = io.KeySuper || (io.KeyCtrl != io.KeyAlt);
    if (atlasKeyboardFocus && !shortcutModifier && !io.WantTextInput &&
        !ImGui::IsAnyItemActive() &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
        !s.fonts.empty() && !io.InputQueueCharacters.empty()) {
        const auto glyphs = s.registry().inspect(s.context.font);
        for (ImWchar code : io.InputQueueCharacters) {
            const auto found = glyphs.glyphs.find(static_cast<unsigned>(code));
            if (found == glyphs.glyphs.end()) {
                s.log("No glyph for " + GuiStudio::character(code));
                continue;
            }
            s.selectedGlyph = static_cast<unsigned>(code);
            const auto &textures = s.atlasTextures[size_t(s.selectedFont)];
            for (size_t i = 0; i < textures.size(); ++i) {
                if (textures[i]->id == found->second.texture &&
                    int(i) != s.selectedAtlas) {
                    s.selectedAtlas = int(i);
                    pan = {0, 0};
                    fitAtlas = true;
                    break;
                }
            }
        }
    }
    const float remaining = ImGui::GetContentRegionAvail().y;
    const float maxPreview = std::max(130.f, remaining - 100.f);
    const float reserved = std::clamp(previewHeight, 130.f, maxPreview);
    if (!s.fonts.empty()) {
        const auto &font = s.fonts[size_t(s.selectedFont)];
        const auto &definition = font.atlases[size_t(s.selectedAtlas)];
        const auto &resource =
            *s.atlasTextures[size_t(s.selectedFont)][size_t(s.selectedAtlas)];
        const auto &im = resource.image;
        const auto start = ImGui::GetCursorScreenPos();
        auto available = ImGui::GetContentRegionAvail();
        available.y = std::max(60.f, available.y - reserved);
        if (fitAtlas) {
            zoom = std::min(available.x / im.Width, available.y / im.Height);
            pan = {0, 0};
            fitAtlas = false;
        }
        ImGui::InvisibleButton("atlas-canvas", available,
                               ImGuiButtonFlags_MouseButtonLeft |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(2)) {
            atlasKeyboardFocus = hovered;
        }
        ImGui::SetItemTooltip(
            "Type letters, numbers or symbols to select their glyph.\n"
            "Shift and composed accents follow your keyboard layout.\n"
            "Escape exits keyboard selection. Middle drag pans; wheel zooms.");
        if (hovered && ImGui::IsMouseDragging(2)) {
            pan.x += ImGui::GetIO().MouseDelta.x;
            pan.y += ImGui::GetIO().MouseDelta.y;
        }
        if (hovered && ImGui::GetIO().MouseWheel) {
            zoom = std::clamp(zoom * std::pow(1.1f, ImGui::GetIO().MouseWheel),
                              .05f, 5.f);
        }
        const ImVec2 origin{start.x + pan.x, start.y + pan.y};
        const float width = im.Width * zoom, height = im.Height * zoom;
        auto *draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(
            start, {start.x + available.x, start.y + available.y}, true);
        for (int y = 0; y < int(available.y); y += 16) {
            for (int x = 0; x < int(available.x); x += 16) {
                const auto c = ((x / 16 + y / 16) & 1)
                                   ? IM_COL32(60, 60, 65, 255)
                                   : IM_COL32(35, 35, 40, 255);
                draw->AddRectFilled({start.x + x, start.y + y},
                                    {start.x + x + 16, start.y + y + 16}, c);
            }
        }
        draw->AddImage(ImTextureRef(ImTextureID(resource.id)), origin,
                       {origin.x + width, origin.y + height}, {0, 1}, {1, 0});
        const float cellW = width / definition.grid.columns;
        const float cellH = height / definition.grid.rows;
        if (grid) {
            for (int col = 0; col <= definition.grid.columns; ++col) {
                draw->AddLine({origin.x + col * cellW, origin.y},
                              {origin.x + col * cellW, origin.y + height},
                              IM_COL32(70, 140, 255, 170));
            }
            for (int row = 0; row <= definition.grid.rows; ++row) {
                draw->AddLine({origin.x, origin.y + row * cellH},
                              {origin.x + width, origin.y + row * cellH},
                              IM_COL32(70, 140, 255, 170));
            }
        }
        if (inspectedGeneration != s.fontGeneration ||
            inspectedRevision != s.registry().revision() ||
            inspectedFont != font.name) {
            inspection = s.registry().inspect(font.name);
            inspectedGeneration = s.fontGeneration;
            inspectedRevision = s.registry().revision();
            inspectedFont = font.name;
        }
        for (const auto &entry : inspection.glyphs) {
            const auto &g = entry.second;
            if (g.texture != resource.id) {
                continue;
            }
            const ImVec2 cell{origin.x + float(g.u0) * width,
                              origin.y + float(1 - g.v1) * height};
            if (bounds && g.visible) {
                draw->AddRect({cell.x + float(g.left) * cellH,
                               cell.y + cellH - float(g.top) * cellH},
                              {cell.x + float(g.right) * cellH,
                               cell.y + cellH - float(g.bottom) * cellH},
                              IM_COL32(60, 255, 140, 255));
            }
            if (entry.first == s.selectedGlyph) {
                draw->AddRect(cell, {cell.x + cellW, cell.y + cellH},
                              IM_COL32(255, 190, 40, 255), 0.f, 2.f,
                              ImDrawFlags_None);
            }
            const auto mouse = ImGui::GetMousePos();
            if (hovered && ImGui::IsMouseClicked(0) && mouse.x >= cell.x &&
                mouse.x < cell.x + cellW && mouse.y >= cell.y &&
                mouse.y < cell.y + cellH) {
                s.selectedGlyph = entry.first;
            }
        }
        draw->PopClipRect();
    }
    const auto divider = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("atlas-preview-divider",
                           {ImGui::GetContentRegionAvail().x, 6.f});
    const bool resizing = ImGui::IsItemActive();
    if (resizing || ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (resizing) {
        previewHeight = std::clamp(reserved - ImGui::GetIO().MouseDelta.y,
                                   130.f, maxPreview);
    }
    ImGui::SetItemTooltip("Drag to resize the text preview");
    ImGui::GetWindowDrawList()->AddRectFilled(
        divider, {divider.x + ImGui::GetItemRectSize().x, divider.y + 6.f},
        resizing ? IM_COL32(80, 150, 210, 255) : IM_COL32(55, 70, 85, 255));
    const float previewWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(180.f);
    ImGui::SliderFloat("Preview size", &fontDraw.fontPixels, 16.f, 128.f,
                       "%.0f px");
    ImGui::SameLine();
    if (ImGui::Button("Fit text")) {
        CFonts::TextLayout layout;
        s.registry().layoutText(sample, layout, s.context.font);
        if (layout.right > 0) {
            fontDraw.fontPixels = std::clamp(
                float((previewWidth - 16.f) / layout.right), 16.f, 128.f);
        }
    }
    ImGui::InputText("Preview text", &sample);
    fontDraw.text = sample;
    auto size = ImGui::GetContentRegionAvail();
    previewItem(fontDraw, {std::max(100.f, size.x), std::max(90.f, size.y)});
}

void UI::draw(int initialPanel) {
    // A confirmed import must not destroy textures referenced by the previous
    // frame's still-pending ImGui draw commands.
    if (deferred) {
        auto operation = std::move(deferred);
        deferred = {};
        attempt(std::move(operation));
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    const auto &io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("GUI Studio", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("Guitar++ GUI Studio   %s%s",
                session.dirtyMenus ? "Menus modified  " : "",
                session.dirtyFonts ? "Fonts modified" : "");
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 70);
    if (ImGui::Button("Quit")) {
        requestClose();
    }
    if (ImGui::BeginTabBar("Studios")) {
        if (ImGui::BeginTabItem(
                "Menu Studio", nullptr,
                initialPanel == 0 ? ImGuiTabItemFlags_SetSelected : 0)) {
            atlasKeyboardFocus = false;
            if (ImGui::BeginTable("menu-layout", 2,
                                  ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Inspector",
                                        ImGuiTableColumnFlags_WidthFixed, 440);
                ImGui::TableSetupColumn("Preview");
                ImGui::TableNextColumn();
                ImGui::BeginChild("menu-inspector", {0, -90});
                menus();
                ImGui::EndChild();
                ImGui::TableNextColumn();
                ImGui::BeginChild("menu-canvas", {0, -90});
                canvas();
                ImGui::EndChild();
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(
                "Font Studio", nullptr,
                initialPanel == 1 ? ImGuiTabItemFlags_SetSelected : 0)) {
            if (previewFocus) {
                focus(false);
            }
            if (ImGui::BeginTable("font-layout", 2,
                                  ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Inspector",
                                        ImGuiTableColumnFlags_WidthFixed, 440);
                ImGui::TableSetupColumn("Atlas");
                ImGui::TableNextColumn();
                ImGui::BeginChild("font-inspector", {0, -90});
                ImGui::PushItemWidth(
                    std::max(100.f, ImGui::GetContentRegionAvail().x - 180));
                fonts();
                ImGui::PopItemWidth();
                ImGui::EndChild();
                ImGui::TableNextColumn();
                ImGui::BeginChild("font-atlas", {0, -90});
                atlas();
                ImGui::EndChild();
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator();
    ImGui::TextWrapped("Assets: %s | Output: %s",
                       session.assets.string().c_str(),
                       session.output.string().c_str());
    if (!session.error.empty()) {
        ImGui::TextColored({1, .5f, .35f, 1}, "%s", session.error.c_str());
    } else if (!session.events.empty()) {
        ImGui::TextWrapped("Last event: %s", session.events.back().c_str());
    }
    if (pending) {
        ImGui::OpenPopup("Unsaved changes");
    }
    if (ImGui::BeginPopupModal("Unsaved changes", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(
            "There are unexported changes. Continue without saving?");
        if (ImGui::Button("Continue")) {
            deferred = std::move(pending);
            pending = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            pending = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::End();
    ImGui::Render();
}
} // namespace GuiStudio
