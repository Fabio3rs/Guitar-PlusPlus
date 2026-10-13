#include "CUIOverrides.h"
#include "CLog.h"

namespace ui {
namespace {
template <class T> void assign(T &target, const std::optional<T> &source) {
    if (source) {
        target = *source;
    }
}

template <class T>
void overlay(std::optional<T> &target, const std::optional<T> &source) {
    if (source) {
        target = source;
    }
}

void merge(OptionStyleOverrides &target, const OptionStyleOverrides &source) {
    overlay(target.textSize, source.textSize);
    overlay(target.textColor, source.textColor);
    overlay(target.background, source.background);
    overlay(target.border, source.border);
    overlay(target.borderWidth, source.borderWidth);
}

void merge(StyleOverrides &target, const StyleOverrides &source) {
    overlay(target.textSize, source.textSize);
    overlay(target.rowStep, source.rowStep);
    overlay(target.padding, source.padding);
    overlay(target.pickGap, source.pickGap);
    overlay(target.textColor, source.textColor);
    overlay(target.accent, source.accent);
    overlay(target.selection, source.selection);
    overlay(target.focus, source.focus);
    overlay(target.focusBackground, source.focusBackground);
    overlay(target.field, source.field);
    overlay(target.control, source.control);
    overlay(target.panel, source.panel);
    overlay(target.animateSelection, source.animateSelection);
    overlay(target.panelBorder, source.panelBorder);
    overlay(target.panelBorderWidth, source.panelBorderWidth);
}

void panel(CMenu &menu, const CMenu::Rect &rect) {
    menu.qbgd.x1 = menu.qbgd.x4 = rect.left;
    menu.qbgd.x2 = menu.qbgd.x3 = rect.right;
    menu.qbgd.y1 = menu.qbgd.y2 = rect.top;
    menu.qbgd.y3 = menu.qbgd.y4 = rect.bottom;
}
} // namespace

OptionPresentation captureOption(const CMenu::menuOpt &o) {
    return {o.x,           o.y,
            o.fieldWidth,  o.deslizantBarSize,
            o.anchorRight, o.manualPosition,
            o.centerX,     o.styleOverrides,
            o.width,       o.height};
}

MenuPresentation capturePresentation(const CMenu &menu) {
    MenuPresentation result{
        menu.x,
        menu.y,
        menu.flowX,
        menu.flowY,
        menu.leftMargin,
        menu.flowGroup,
        menu.flow,
        menu.viewportEnabled,
        menu.anchorLeft,
        menu.viewport,
        {menu.qbgd.x1, menu.qbgd.y3, menu.qbgd.x2, menu.qbgd.y1},
        menu.styleOverrides,
        {},
        menu.tableColumns,
        menu.getFixedPanelBounds()};

    for (const auto &option : menu.options) {
        result.options.emplace(option.optionName, captureOption(option));
    }

    return result;
}

void restorePresentation(CMenu &menu, const MenuPresentation &base) {
    menu.x = base.x;
    menu.y = base.y;
    menu.flowX = base.flowX;
    menu.flowY = base.flowY;
    menu.leftMargin = base.leftMargin;
    menu.flowGroup = base.flowGroup;
    menu.flow = base.flow;

    menu.viewportEnabled = base.viewportEnabled;
    menu.anchorLeft = base.anchorLeft;
    menu.viewport = base.viewport;
    menu.setPanelBounds(base.panelBounds);

    menu.styleOverrides = base.style;
    menu.tableColumns = base.tableColumns;
    panel(menu, base.panel);

    for (auto &o : menu.options) {
        const auto found = base.options.find(o.optionName);
        if (found == base.options.end()) {
            continue;
        }

        const auto &b = found->second;
        o.x = b.x;
        o.y = b.y;
        o.fieldWidth = b.fieldWidth;
        o.deslizantBarSize = b.listWidth;
        o.anchorRight = b.anchorRight;
        o.manualPosition = b.manual;
        o.centerX = b.centerX;
        o.styleOverrides = b.style;
        o.width = b.width;
        o.height = b.height;

        o.fixedX.reset();
        o.fixedY.reset();
        o.overrideHorizontal = false;
    }
}

void applyOption(CMenu::menuOpt &o, const OptionLayoutOverrides &patch) {
    assign(o.fieldWidth, patch.fieldWidth);
    assign(o.width, patch.width);
    assign(o.height, patch.height);
    assign(o.deslizantBarSize, patch.listWidth);

    o.fixedX = patch.x;
    o.fixedY = patch.y;
    assign(o.x, patch.x);
    assign(o.y, patch.y);

    if (patch.x || patch.centerX || patch.anchorRight.value_or(false)) {
        o.anchorRight = false;
        o.centerX.reset();
    }
    if (patch.centerX) {
        o.centerX = patch.centerX;
    }

    assign(o.anchorRight, patch.anchorRight);
    o.overrideHorizontal = patch.x.has_value() || patch.centerX.has_value() ||
                           patch.anchorRight.has_value();

    merge(o.styleOverrides, patch.style);
}

void applyMenu(CMenu &menu, const MenuOverrides &patch) {
    assign(menu.x, patch.x);
    assign(menu.tableColumns, patch.tableColumns);
    assign(menu.y, patch.y);

    if (patch.flowX) {
        menu.anchorLeft = false;
    }

    assign(menu.flowX, patch.flowX);
    assign(menu.flowY, patch.flowY);
    assign(menu.leftMargin, patch.leftMargin);
    assign(menu.flowGroup, patch.flowGroup);
    assign(menu.flow, patch.flow);

    assign(menu.viewportEnabled, patch.viewportEnabled);
    assign(menu.anchorLeft, patch.anchorLeft);
    assign(menu.viewport, patch.viewport);

    if (patch.panelFollowsViewport.value_or(false)) {
        menu.setPanelBounds(std::nullopt);
    }
    if (patch.panelBounds) {
        menu.setPanelBounds(patch.panelBounds);
    }

    if (patch.panel) {
        panel(menu, *patch.panel);
    }

    merge(menu.styleOverrides, patch.style);

    for (const auto &entry : patch.options) {
        if (auto *option = menu.findOption(entry.first)) {
            applyOption(*option, entry.second);
        } else {
            CLog::log() << "Unknown presentation option: " + menu.getName() +
                               "." + entry.first;
        }
    }
}
} // namespace ui
