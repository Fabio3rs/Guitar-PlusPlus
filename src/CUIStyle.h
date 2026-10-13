#pragma once

#include <optional>

namespace ui {

struct Color {
    double r = 1, g = 1, b = 1, a = 1;
};

struct Typography {
    double textSize = .075;
    Color textColor;
};

struct LayoutStyle {
    double rowStep = .1, padding = .08;
};

struct SelectionStyle {
    Color accent{1, .88, .5, 1};
    Color background{.04, .04, .055, .72};
    double pickGap = .2;
    bool animated = false;
};

struct ControlStyle {
    Color field{.025, .025, .035, .82};
    Color track{.15, .17, .21, .9};
};

struct PanelStyle {
    Color background{.07, .08, .10, .95};
    Color border{.3, .32, .36, 1};
    double borderWidth = 0;
};

struct Style {
    Typography typography;
    LayoutStyle layout;
    SelectionStyle selection;
    ControlStyle controls;
    PanelStyle panel;
};

// Unset properties keep following the active style, including on open menus.
struct StyleOverrides {
    std::optional<double> textSize, rowStep, padding, pickGap;
    std::optional<Color> textColor, accent, selection, field, control, panel;
    std::optional<bool> animateSelection;
    std::optional<Color> panelBorder;
    std::optional<double> panelBorderWidth;
};

struct OptionStyleOverrides {
    std::optional<double> textSize;
    std::optional<Color> textColor;
    std::optional<Color> background, border;
    std::optional<double> borderWidth;
};

// UI/main-thread only. No GL resources or dependency on game initialization.
const Style &activeStyle();
void setActiveStyle(const Style &style);
Style resolveStyle(const Style &base, const StyleOverrides &overrides);
Typography resolveTypography(const Typography &base,
                             const OptionStyleOverrides &overrides);

} // namespace ui
