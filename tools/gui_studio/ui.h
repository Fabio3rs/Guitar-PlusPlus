#pragma once
#include "session.h"
#include <imgui.h>

namespace GuiStudio {
struct PreviewDraw {
    Session *session = nullptr;
    ImVec2 position, size;
    bool font = false;
    float fontPixels = 48.f;
    std::string text;
};
class UI {
    Session &session;
    GLFWwindow *window;
    std::string ini, menuFile, fontFile, exportMenu = "Gui.lua",
                                         exportFont = "Theme.lua";
    bool showExportResult = false, exportSucceeded = false;
    std::string exportedPath, exportError;
    void exportFile(bool font);
    std::string sample = "Guitar++ ação ÁÉÍÓÚ ç gpqy 0123456789";
    std::string newName = "new-font", newFile, newMapping = "ABC\nabc";
    int newColumns = 3, newRows = 2;
    bool addAtlas = false, previewFocus = false, mouseCaptured = false;
    bool grid = true, bounds = true, fitAtlas = true;
    bool atlasKeyboardFocus = false;
    float zoom = .65f;
    float previewHeight = 240.f;
    ImVec2 pan{0, 0};
    PreviewDraw menuDraw, fontDraw;
    CFonts::Inspection inspection;
    size_t inspectedGeneration = 0;
    size_t inspectedRevision = ~size_t(0);
    std::string inspectedFont;
    std::function<void()> pending, deferred;
    void menus();
    void fonts();
    void canvas();
    void atlas();
    void protect(std::function<void()>);
    void attempt(std::function<void()>);

  public:
    bool closeApproved = false;
    UI(Session &, GLFWwindow *);
    ~UI();
    void draw(int initialPanel = -1);
    void key(int key, int action, int mods);
    void character(unsigned);
    void focus(bool);
    void requestClose();
};
} // namespace GuiStudio
