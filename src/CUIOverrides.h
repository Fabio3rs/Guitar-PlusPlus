#pragma once
#include "CMenu.h"
#include <filesystem>

namespace ui {
// Presentation-only patches. Absence means inheritance, never a default value.
struct OptionLayoutOverrides {
    std::optional<double> x, y, fieldWidth, listWidth, centerX;
    std::optional<bool> anchorRight;
    OptionStyleOverrides style;
    std::optional<double> width, height;
};

struct MenuOverrides {
    std::optional<double> x, y, flowX, flowY, leftMargin;
    std::optional<int> flowGroup;
    std::optional<CMenu::Flow> flow;
    std::optional<bool> viewportEnabled, anchorLeft;
    std::optional<CMenu::Rect> viewport, panel;
    std::optional<CMenu::Rect> panelBounds;
    std::optional<bool> panelFollowsViewport;
    StyleOverrides style;
    std::map<std::string, OptionLayoutOverrides> options;
    std::optional<std::vector<double>> tableColumns;
};
using MenuOverrideSet = std::map<std::string, MenuOverrides>;

// Owned values only: no callbacks, text, GL objects, focus or user input.
struct OptionPresentation {
    double x, y, fieldWidth, listWidth;
    bool anchorRight, manual;
    std::optional<double> centerX;
    OptionStyleOverrides style;
    double width = 0, height = 0;
};
struct MenuPresentation {
    double x, y, flowX, flowY, leftMargin;
    int flowGroup;
    CMenu::Flow flow;
    bool viewportEnabled, anchorLeft;
    CMenu::Rect viewport, panel;
    StyleOverrides style;
    std::map<std::string, OptionPresentation> options;
    std::vector<double> tableColumns;
    std::optional<CMenu::Rect> panelBounds;
};

MenuOverrideSet readOverrides(const std::filesystem::path &);
void writeOverrides(const std::filesystem::path &, const MenuOverrideSet &);
void validateOverrides(const MenuOverrideSet &);
OptionPresentation captureOption(const CMenu::menuOpt &);
MenuPresentation capturePresentation(const CMenu &);
void restorePresentation(CMenu &, const MenuPresentation &);
void applyOption(CMenu::menuOpt &, const OptionLayoutOverrides &);
void applyMenu(CMenu &, const MenuOverrides &);
} // namespace ui
