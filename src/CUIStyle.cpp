#include "CUIStyle.h"
#include <cmath>

namespace ui {
namespace {
Style &storage() {
    static Style style;
    return style;
}

double positiveSize(double value, double fallback) {
    return std::isfinite(value) && value > 0 ? value : fallback;
}
} // namespace

const Style &activeStyle() { return storage(); }

void setActiveStyle(const Style &style) {
    storage() = style;
    storage().typography.textSize =
        positiveSize(style.typography.textSize, .075);
}

Style resolveStyle(const Style &base, const StyleOverrides &overrides) {
    Style result = base;
    OptionStyleOverrides typography;
    typography.textSize = overrides.textSize;
    typography.textColor = overrides.textColor;
    result.typography = resolveTypography(base.typography, typography);
    result.layout.rowStep = overrides.rowStep.value_or(base.layout.rowStep);
    result.layout.padding = overrides.padding.value_or(base.layout.padding);
    result.selection.pickGap =
        overrides.pickGap.value_or(base.selection.pickGap);
    result.selection.accent =
        overrides.accent.value_or(base.selection.accent);
    result.selection.background =
        overrides.selection.value_or(base.selection.background);
    result.selection.animated =
        overrides.animateSelection.value_or(base.selection.animated);
    result.controls.field = overrides.field.value_or(base.controls.field);
    result.controls.track = overrides.control.value_or(base.controls.track);
    result.panel.background = overrides.panel.value_or(base.panel.background);
    result.panel.border = overrides.panelBorder.value_or(base.panel.border);
    result.panel.borderWidth =
        overrides.panelBorderWidth.value_or(base.panel.borderWidth);
    return result;
}

Typography resolveTypography(const Typography &base,
                             const OptionStyleOverrides &overrides) {
    const double inherited = positiveSize(base.textSize, .075);
    return {positiveSize(overrides.textSize.value_or(inherited), inherited),
            overrides.textColor.value_or(base.textColor)};
}

} // namespace ui
