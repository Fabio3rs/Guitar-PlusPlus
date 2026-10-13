#pragma once
#include "CMenu.h"
#include <filesystem>

namespace ui {

// Persistent descriptions only: no callbacks, GL handles or interaction state.
struct OptionDefinition {
    std::string id, text, value, languageKey, texture;
    CMenu::menusOPT type = CMenu::textbtn;
    int group = 1, shortcut = 0;
    double x = 0, y = 0, fieldWidth = .55, listWidth = .5;
    double width = 0, height = 0;
    bool active = false;
    bool ellipsizeCells = false, activateOnClick = true;
    std::vector<std::string> cells;
    bool disabled = false, manual = false, anchorRight = false, back = false;
    bool enableEnter = true;
    std::optional<double> centerX;
    size_t maxBytes = 30;
    std::vector<std::string> items, actions;
    OptionStyleOverrides style;
};

struct MenuDefinition {
    std::string id, title, background;
    bool modal = false, viewportEnabled = false, anchorLeft = false;
    double x = 0, y = 0, flowX = 0, flowY = .4, leftMargin = .2;
    int flowGroup = 1;
    CMenu::Flow flow = CMenu::Flow::Manual;
    CMenu::Rect viewport{-1, -.75, 1, .75};
    CMenu::Rect panel{-.1, -.6, 1.4, .15};
    StyleOverrides style;
    std::vector<OptionDefinition> options;
    std::vector<double> tableColumns;
};

using MenuDefinitions = std::vector<MenuDefinition>;
using TextureResolver = std::function<unsigned(const std::string &)>;
MenuDefinitions defaultModals();
void validate(const MenuDefinitions &);
MenuDefinitions readMenus(const std::filesystem::path &);
void writeMenus(const std::filesystem::path &, const MenuDefinitions &);
std::unique_ptr<CMenu> instantiate(const MenuDefinition &,
                                   CMenu::Context * = nullptr,
                                   const TextureResolver & = {});
std::string luaQuote(const std::string &);
void writeTextFile(const std::filesystem::path &, const std::string &);

} // namespace ui
