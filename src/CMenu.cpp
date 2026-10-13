#include "CMenu.h"
#include "CLanguageManager.h"
#include "CLog.h"
#include "CUIOverrides.h"
#include "GPPGame.h"
#include "utf8.h"
#include <GL/glew.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <tuple>

std::deque<CMenu::uiWindowStruct> CMenu::uiList;
std::vector<CMenu::posUiOrder> CMenu::uiOrderList;
int CMenu::menusCreated = 0;
CMenu *CMenu::inputFocus = nullptr;
CMenu *CMenu::mouseCapture = nullptr;

namespace {
void rectangle(const CMenu::Rect &r, unsigned texture = 0) {
    if (texture) {
        glEnable(GL_TEXTURE_2D);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
    CEngine::RenderDoubleStruct q{};
    q.x1 = q.x4 = r.left;
    q.x2 = q.x3 = r.right;
    q.y1 = q.y2 = r.top;
    q.y3 = q.y4 = r.bottom;
    q.TextureX1 = 0;
    q.TextureX2 = 1;
    q.TextureY1 = 1;
    q.TextureY2 = 0;
    q.Text = texture;
    CEngine::engine().Render2DQuad(q);
}

bool interactive(const CMenu::menuOpt &o) {
    return !o.disabled && o.type != CMenu::static_text &&
           o.type != CMenu::none && o.type != CMenu::image &&
           o.type != CMenu::panel;
}

bool keyboardFocusable(const CMenu::menuOpt &o) {
    return interactive(o) && o.type != CMenu::drag_bar;
}

void clampList(CMenu::menuOpt &o) {
    o.listID =
        o.optList.empty()
            ? 0
            : std::clamp(o.listID, 0, static_cast<int>(o.optList.size() - 1));
}
} // namespace

void CMenu::menuOpt::update() {
    if (langEntryKey.empty()) {
        return;
    }

    const auto translated = CLanguageManager::langMGR().getText(
        GPPGame::GuitarPP().glanguage, langEntryKey);

    if (!translated.empty()) {
        text = translated;
    }
}

CMenu::CMenu() : CMenu("menu_main_" + std::to_string(++menusCreated)) {}

CMenu::CMenu(const std::string &name, Context *host)
    : menuName(name), uiMenu(false), thisUiID(-1), lastEnterOptBtn(false),
      devMenuNOUpdateOthers(false), lScript(nullptr), devEditMenu(nullptr),
      devEditingOpt(0), temp(false), gameMenu(false), mouseAX(0), mouseAY(0),
      status(0) {
    context = host;
    x = y = 0;
}

CMenu::CMenu(const CMenu &m) : CMenu(m.menuName, m.context) {
    nextOptionId = m.nextOptionId;
    if (m.presentationBase) {
        presentationBase =
            std::make_unique<ui::MenuPresentation>(*m.presentationBase);
    }
    if (m.presentationPatch) {
        presentationPatch =
            std::make_unique<ui::MenuOverrides>(*m.presentationPatch);
    }
    x = m.x;
    y = m.y;
    styleOverrides = m.styleOverrides;
    flow = m.flow;
    tableColumns = m.tableColumns;
    modal = m.modal;
    viewportEnabled = m.viewportEnabled;
    viewport = m.viewport;
    fixedPanelBounds = m.fixedPanelBounds;
    flowX = m.flowX;
    flowY = m.flowY;
    flowGroup = m.flowGroup;
    anchorLeft = m.anchorLeft;
    leftMargin = m.leftMargin;
    mTitle = m.mTitle;
    qbgd = m.qbgd;
    backgroundTexture = m.backgroundTexture;
    gameMenu = m.gameMenu;
    options = m.options;
    groupInfo = m.groupInfo;
    openCallback = m.openCallback;
    luaCallBacks = m.luaCallBacks;

    for (auto &o : options) {
        o.externalPreTextRef = nullptr;
        o.attachedLua = nullptr;
        o.status = o.devStatus = 0;
        o.btnClickStat = false;
        o.pressingShortcutKey = 0;
    }
}

CMenu::~CMenu() {
    resetBtns();
    closeInterfaces();
    if (inputFocus == this) {
        inputFocus = returnFocus ? returnFocus : owner;
    }
    // Saved focus must not outlive its target, including when a sibling closes.
    for (auto &slot : uiList) {
        if (slot.m && slot.m->returnFocus == this) {
            slot.m->returnFocus = owner;
        }
    }
}

CMenu *CMenu::rootMenu() { return owner ? owner->rootMenu() : this; }

bool CMenu::containsInterface(const CMenu *menu) const {
    for (const auto *candidate = menu; candidate != nullptr;
         candidate = candidate->owner) {
        if (candidate == this) {
            return true;
        }
    }
    return false;
}

bool CMenu::hasFocusableOptions() const {
    return std::any_of(options.begin(), options.end(), keyboardFocusable);
}

CMenu *CMenu::blockingModal() {
    interfaceCalcPos();
    CMenu *root = rootMenu();

    for (auto it = uiOrderList.rbegin(); it != uiOrderList.rend(); ++it) {
        auto *menu = uiList[static_cast<size_t>(it->id)].m.get();
        if (menu->modal && root->containsInterface(menu)) {
            return menu;
        }
    }

    return root->modal ? root : nullptr;
}

CMenu *CMenu::focusedInterface() {
    return rootMenu()->containsInterface(inputFocus) ? inputFocus : nullptr;
}

bool CMenu::focusInterface(CMenu &menu) {
    if (!rootMenu()->containsInterface(&menu) || !menu.hasFocusableOptions()) {
        return false;
    }

    auto *modalMenu = blockingModal();
    if (modalMenu && !modalMenu->containsInterface(&menu)) {
        return false;
    }

    if (inputFocus && inputFocus != &menu && inputFocus->focused >= 0 &&
        static_cast<size_t>(inputFocus->focused) < inputFocus->options.size()) {
        inputFocus->options[static_cast<size_t>(inputFocus->focused)].status &=
            ~4;
    }

    inputFocus = &menu;
    if (menu.focused < 0 ||
        static_cast<size_t>(menu.focused) >= menu.options.size() ||
        !keyboardFocusable(menu.options[static_cast<size_t>(menu.focused)])) {
        for (size_t i = 0; i < menu.options.size(); ++i) {
            if (keyboardFocusable(menu.options[i])) {
                menu.select(static_cast<int>(i));
                break;
            }
        }
    }

    return true;
}

bool CMenu::focusNextPanel(bool backwards) {
    auto *scope = blockingModal();
    if (!scope) {
        scope = rootMenu();
    }

    std::vector<CMenu *> regions;
    if (scope->hasFocusableOptions()) {
        regions.push_back(scope);
    }

    bool composed = false;
    scope->visitInterfaces([&](CMenu &child) {
        if (child.permanentPanel) {
            composed = true;
            if (child.hasFocusableOptions()) {
                regions.push_back(&child);
            }
        }
    });

    if (!composed) {
        return false;
    }

    if (!regions.empty()) {
        const auto current =
            std::find(regions.begin(), regions.end(), inputFocus);
        size_t index = backwards ? regions.size() - 1 : 0;
        if (current != regions.end()) {
            const auto position =
                static_cast<size_t>(current - regions.begin());
            index = backwards ? (position + regions.size() - 1) % regions.size()
                              : (position + 1) % regions.size();
        }
        focusInterface(*regions[index]);
    }

    return true;
}

bool CMenu::hitOption(size_t index, double px, double py,
                      const ui::Style &style) {
    const auto panelBounds = getPanelBounds();
    if (permanentPanel && !Rect{panelBounds.left + x, panelBounds.bottom + y,
                                panelBounds.right + x, panelBounds.top + y}
                               .contains(px, py)) {
        return false;
    }
    if (viewportEnabled && options[index].group == flowGroup &&
        !Rect{viewport.left + x, viewport.bottom + y, viewport.right + x,
              viewport.top + y}
             .contains(px, py)) {
        return false;
    }
    const auto r = optionRect(index, style);
    return r.right > r.left && r.top > r.bottom && r.contains(px, py);
}

bool CMenu::hitRegion(double px, double py) {
    const auto panelBounds = getPanelBounds();
    if (permanentPanel) {
        return Rect{panelBounds.left + x, panelBounds.bottom + y,
                    panelBounds.right + x, panelBounds.top + y}
            .contains(px, py);
    }
    if ((viewportEnabled && Rect{viewport.left + x, viewport.bottom + y,
                                 viewport.right + x, viewport.top + y}
                                .contains(px, py)) ||
        (qbgd.Text &&
         Rect{qbgd.x1 + x, qbgd.y3 + y, qbgd.x2 + x, qbgd.y1 + y}.contains(
             px, py))) {
        return true;
    }
    const auto style = resolvedStyle();
    for (size_t i = 0; i < options.size(); ++i) {
        if (hitOption(i, px, py, style)) {
            return true;
        }
    }
    return false;
}

void CMenu::closeInterfaces() {
    // Detach ownership before recursively destroying children.
    auto children = std::move(myUiList);
    myUiList.clear();

    for (int id : children) {
        if (id >= 0 && static_cast<size_t>(id) < uiList.size()) {
            auto child = std::move(uiList[static_cast<size_t>(id)].m);
            uiList[static_cast<size_t>(id)].pos = 0;
        }
    }

}

int CMenu::addOpt(const menuOpt &opt) {
    const size_t index = options.size();

    if (index >= static_cast<size_t>(std::numeric_limits<int>::max())) {
        return -1;
    }

    const std::string prefix = menuName + ".";
    std::string name = opt.optionName;
    if (name.empty()) {
        do {
            name = prefix + "auto_" + std::to_string(nextOptionId++);
        } while (findOption(name));
    } else if (name.compare(0, prefix.size(), prefix) != 0) {
        name = prefix + name;
    }
    if (findOption(name)) {
        CLog::log() << "Duplicate menu option: " + name;
        return -1;
    }
    options.push_back(opt);
    auto &o = options.back();
    translate(o);
    o.optionName = std::move(name);
    o.eventID = context ? 0 : CLuaH::Lua().idForCallbackEvent(o.optionName);
    if (presentationBase) {
        presentationBase->options[o.optionName] = ui::captureOption(o);
        if (presentationPatch) {
            const auto local = o.optionName.substr(prefix.size());
            const auto patch = presentationPatch->options.find(local);
            if (patch != presentationPatch->options.end()) {
                ui::applyOption(o, patch->second);
            }
        }
    }
    groupInfo.try_emplace(o.group);
    return static_cast<int>(index);
}

void CMenu::resetData() {
    resetBtns();
    for (auto &o : options) {
        o.status = 0;
        o.devStatus = 0;
        o.btnClickStat = false;
    }

    focused = hovered = captured = -1;
    focusGeometryOption = -1;
    dragging = false;
    status = 0;
}

void CMenu::resetBtns() {
    if (mouseCapture == this) {
        mouseCapture = nullptr;
    }
    captured = -1;
    dragging = false;
    lastEnterOptBtn = lastMouseClickStatus = false;
    status = 0;

    for (auto &o : options) {
        o.btnClickStat = false;
        o.status &= ~2;
        o.pressingShortcutKey = 0;
    }
}

void CMenu::ensureLayout(menuOpt &o) {
    if (o.type == table_row || o.type == image || o.type == panel) {
        return;
    }
    auto &fonts = fontService();
    const auto &text = o.type == text_input ? o.preText : o.text;

    if (o.cachedRevision != fonts.revision() || o.cachedText != text) {
        fonts.layoutText(text, o.layout, context ? context->font : "default");
        o.cachedText = text;
        o.cachedRevision = fonts.revision();
    }

    clampList(o);
    const std::string empty;
    const auto &value =
        o.optList.empty() ? empty : o.optList[static_cast<size_t>(o.listID)];

    if (o.type == deslizant_Select_list &&
        (o.valueRevision != fonts.revision() || o.cachedValue != value)) {
        fonts.layoutText(value, o.valueLayout,
                         context ? context->font : "default");
        o.cachedValue = value;
        o.valueRevision = fonts.revision();
    }

    if (!std::isfinite(o.deslizantBarSize) || o.deslizantBarSize < 0) {
        o.deslizantBarSize = 0;
    }

    if (!std::isfinite(o.fieldWidth) || o.fieldWidth <= 0) {
        o.fieldWidth = .55;
    }
}

ui::Style CMenu::resolvedStyle() const {
    return ui::resolveStyle(context ? context->style : ui::activeStyle(),
                            styleOverrides);
}

ui::Typography CMenu::resolvedOptionStyle(const menuOpt &option) const {
    return ui::resolveTypography(resolvedStyle().typography,
                                 option.styleOverrides);
}

CMenu::Rect CMenu::controlRect(size_t i) {
    const auto style = resolvedStyle();
    reflow(style);
    return controlRect(i, style);
}

CMenu::Rect CMenu::optionRect(size_t i) {
    const auto style = resolvedStyle();
    reflow(style);
    return optionRect(i, style);
}

void CMenu::reflow() { reflow(resolvedStyle()); }

CMenu::Rect CMenu::controlRect(size_t i, const ui::Style &style) {
    if (i >= options.size()) {
        return {};
    }

    auto &o = options[i];
    ensureLayout(o);
    const auto appearance =
        ui::resolveTypography(style.typography, o.styleOverrides);
    const double yy =
        o.y + y +
        ((viewportEnabled && o.group == flowGroup) ? scrollOffset : 0);
    double xx = o.x + x;

    if (o.type == deslizant_Select_list) {
        xx += o.layout.right * appearance.textSize + appearance.textSize;
    }

    return {xx, yy, xx + o.deslizantBarSize, yy + appearance.textSize};
}

CMenu::Rect CMenu::layoutRect(menuOpt &o, const ui::Style &style) {
    if (o.type >= table_row ||
        (o.type == textbtn && (o.width > 0 || o.height > 0))) {
        return localRect(o, style);
    }
    ensureLayout(o);
    const auto scale =
        ui::resolveTypography(style.typography, o.styleOverrides).textSize;
    return {o.layout.left * scale, o.layout.bottom * scale,
            o.layout.right * scale, o.layout.top * scale};
}

CMenu::Rect CMenu::localRect(menuOpt &o, const ui::Style &style) {
    auto size = [](double v) { return std::isfinite(v) && v > 0 ? v : 0.0; };
    if (o.type == table_row) {
        double width = 0;
        for (double column : tableColumns) {
            width += size(column);
        }
        return {0, 0, size(width),
                size(o.height) > 0 ? size(o.height)
                                   : size(style.layout.rowStep)};
    }
    if (o.type == image || o.type == panel) {
        return {0, 0, size(o.width), size(o.height)};
    }
    ensureLayout(o);
    const auto appearance =
        ui::resolveTypography(style.typography, o.styleOverrides);
    const double scale = appearance.textSize;
    Rect r{o.layout.left * scale, o.layout.bottom * scale,
           o.layout.right * scale, o.layout.top * scale};
    if (o.type == text_input) {
        r = {0, 0, o.fieldWidth, scale};
    } else if (o.type == drag_bar) {
        r = {0, 0, o.deslizantBarSize, scale};
    } else if (o.type == deslizant_Select_list) {
        r.right +=
            2 * scale + o.deslizantBarSize + o.valueLayout.width() * scale;
    }
    // Legacy glyph bounds stay unpadded here, including for anchors/flow.
    if (o.type == textbtn) {
        if (size(o.width) > 0) {
            r.left = 0;
            r.right = size(o.width);
        }
        if (size(o.height) > 0) {
            r.bottom = 0;
            r.top = size(o.height);
        }
    }
    return r;
}

CMenu::Rect CMenu::optionRect(size_t i, const ui::Style &style) {
    if (i >= options.size()) {
        return {};
    }
    auto &o = options[i];
    auto r = localRect(o, style);
    const double yy =
        o.y + y +
        ((viewportEnabled && o.group == flowGroup) ? scrollOffset : 0);
    const auto appearance =
        ui::resolveTypography(style.typography, o.styleOverrides);
    const bool box = o.type == table_row || o.type == image ||
                     o.type == panel ||
                     (o.type == textbtn && (o.width > 0 || o.height > 0));
    const double pad = box ? 0 : style.layout.padding * appearance.textSize;
    return {r.left + o.x + x - pad, r.bottom + yy - pad,
            r.right + o.x + x + pad, r.top + yy + pad};
}

void CMenu::clampScroll(const ui::Style &style) {
    if (!viewportEnabled) {
        return;
    }
    double lowest = viewport.top;
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i].group == flowGroup) {
            lowest = std::min(lowest,
                              optionRect(i, style).bottom - y - scrollOffset);
        }
    }
    scrollOffset =
        std::clamp(scrollOffset, 0.0, std::max(0.0, viewport.bottom - lowest));
}

void CMenu::setFlow(Flow direction, double originX, double originY, int group) {
    flow = direction;
    flowX = originX;
    flowY = originY;
    flowGroup = group;
}

void CMenu::reflow(const ui::Style &style) {
    for (auto &o : options) {
        if (o.centerX && !o.anchorRight &&
            (flow == Flow::Manual || o.group != flowGroup ||
             o.manualPosition)) {
            const auto r = layoutRect(o, style);
            o.x = *o.centerX - (r.left + r.right) / 2;
        }

        if (o.anchorRight) {
            o.x = aspect() - .1 - layoutRect(o, style).right;
        }
    }

    if (flow == Flow::Manual) {
        applyFixedPositions(style);
        refreshFocusGeometry(style);
        clampScroll(style);
        return;
    }

    if (anchorLeft) {
        flowX = -aspect() + leftMargin;
    }

    double px = flowX, py = flowY;
    std::optional<double> previousBottom;

    for (auto &o : options) {
        if (o.group == flowGroup && !o.manualPosition) {
            const auto r = layoutRect(o, style);
            const auto appearance =
                ui::resolveTypography(style.typography, o.styleOverrides);
            if (flow == Flow::Vertical && previousBottom &&
                (o.type >= table_row || (o.type == textbtn && o.height > 0))) {
                py = std::min(py, *previousBottom - r.top);
            }
            o.x = px;
            o.y = py;
            previousBottom = py + r.bottom;

            if (flow == Flow::Vertical) {
                py -= std::max(
                    style.layout.rowStep,
                    r.top - r.bottom +
                        (o.type >= table_row ? 0 : .3 * appearance.textSize));
            } else {
                px += r.right - r.left +
                      (1 + style.selection.pickGap + .5) * appearance.textSize;
            }
        }
    }

    applyFixedPositions(style);
    refreshFocusGeometry(style);
    clampScroll(style);
}

void CMenu::refreshFocusGeometry(const ui::Style &style) {
    if (!viewportEnabled || focused < 0 ||
        static_cast<size_t>(focused) >= options.size() ||
        options[static_cast<size_t>(focused)].group != flowGroup) {
        focusGeometryOption = -1;
        return;
    }

    auto &o = options[static_cast<size_t>(focused)];
    const auto r = localRect(o, style);
    const auto appearance =
        ui::resolveTypography(style.typography, o.styleOverrides);
    // Exclude scrollOffset: scrolling must not pull focus back into view.
    const std::array<double, 7> geometry{
        o.y,      appearance.textSize, style.layout.padding, r.top,
        r.bottom, viewport.top,        viewport.bottom};
    const bool changed =
        focusGeometryOption == focused && geometry != focusGeometry;
    focusGeometryOption = focused;
    focusGeometry = geometry;

    if (changed) {
        revealFocused(style);
    }
}

void CMenu::select(int i) {
    if (i < 0 || static_cast<size_t>(i) >= options.size() ||
        !interactive(options[static_cast<size_t>(i)])) {
        return;
    }

    for (auto &o : options) {
        // Keep the legacy activation value (3) until callbacks consume it.
        o.status &= ~4;

        if (!(o.status & 2)) {
            o.status &= ~1;
        }
    }

    focused = i;
    auto &o = options[static_cast<size_t>(i)];
    o.status |= 1;

    if (o.group >= 0) {
        groupInfo[o.group].selectedOpt = i;
    }

    revealFocused();
}

void CMenu::revealFocused() {
    const auto style = resolvedStyle();
    reflow(style);
    revealFocused(style);
}

void CMenu::revealFocused(const ui::Style &style) {
    if (!viewportEnabled || focused < 0 ||
        static_cast<size_t>(focused) >= options.size() ||
        options[static_cast<size_t>(focused)].group != flowGroup) {
        return;
    }

    const auto r = optionRect(static_cast<size_t>(focused), style);

    if (r.top > viewport.top + y) {
        scrollOffset -= r.top - (viewport.top + y);
    }

    if (r.bottom < viewport.bottom + y) {
        scrollOffset += viewport.bottom + y - r.bottom;
    }

    scrollOffset = std::max(0.0, scrollOffset);
}

bool CMenu::isMouseOnThisMenu() { return hitRegion(cursorX(), cursorY()); }

bool CMenu::isInterfaceOver() const {
    const auto *root = this;
    while (root->owner != nullptr) {
        root = root->owner;
    }
    interfaceCalcPos();
    for (auto it = uiOrderList.rbegin(); it != uiOrderList.rend(); ++it) {
        auto *menu = uiList[static_cast<size_t>(it->id)].m.get();
        if (menu == this) {
            break;
        }
        if (root->containsInterface(menu) &&
            ((menu->modal && !menu->containsInterface(this)) ||
             menu->isMouseOnThisMenu())) {
            return true;
        }
    }
    return false;
}

bool CMenu::processCommand(const CControls::MenuEvent &event,
                           bool selectOption) {
    if (event.kind != CControls::MenuEvent::Key || event.action != GLFW_PRESS) {
        return false;
    }
    for (size_t i = 0; i < options.size(); ++i) {
        auto &o = options[i];
        if (interactive(o) && o.shortcutKey && o.shortcutKey == event.key) {
            if (selectOption) {
                select(static_cast<int>(i));
            }
            o.status |= 3;
            return true;
        }
    }
    if (event.key == GLFW_KEY_ESCAPE) {
        for (size_t i = 0; i < options.size(); ++i) {
            auto &o = options[i];
            if (interactive(o) && o.goback) {
                if (selectOption) {
                    select(static_cast<int>(i));
                }
                o.status |= 3;
                return true;
            }
        }
    }
    return false;
}

bool CMenu::processEvent(const CControls::MenuEvent &event) {
    const auto style = resolvedStyle();
    reflow(style);
    using Event = CControls::MenuEvent;

    if (event.kind == Event::Focus && !event.action) {
        resetBtns();
        return true;
    }

    if (event.kind == Event::Scroll && viewportEnabled) {
        scrollOffset -= event.y * style.layout.rowStep * 3;
        clampScroll(style);
        return true;
    }

    if (event.kind == Event::Mouse && event.key == GLFW_MOUSE_BUTTON_LEFT) {
        int hit = -1;

        for (size_t i = 0; i < options.size(); ++i) {
            if (interactive(options[i]) &&
                hitOption(i, event.x, event.y, style)) {
                hit = static_cast<int>(i);
            }
        }

        if (event.action == GLFW_PRESS) {
            if (mouseCapture && mouseCapture != this) {
                mouseCapture->resetBtns();
            }
            captured = hit;
            mouseCapture = hit >= 0 ? this : nullptr;

            if (hit < 0) {
                return true;
            }

            select(hit);
            auto &o = options[static_cast<size_t>(hit)];
            const auto appearance =
                ui::resolveTypography(style.typography, o.styleOverrides);
            dragX = event.x;
            dragY = event.y;
            dragging = o.type == drag_bar ||
                       (o.type == deslizant_Select_list &&
                        controlRect(static_cast<size_t>(hit), style)
                            .contains(event.x, event.y));

            if (dragging && o.type == deslizant_Select_list &&
                !o.optList.empty() && o.deslizantBarSize > 0) {
                auto r = controlRect(static_cast<size_t>(hit), style);
                o.listID = static_cast<int>(std::round(
                    std::clamp((event.x - r.left) / o.deslizantBarSize, 0.0,
                               1.0) *
                    static_cast<double>(o.optList.size() - 1)));
            }

            if (o.type == text_input) {
                o.status |= 4;
                o.strEditPoint = static_cast<int>(o.layout.cursorAt(
                    (event.x - x - o.x + o.textScroll) / appearance.textSize));
            }
        } else if (event.action == GLFW_RELEASE) {
            if (captured >= 0 && captured == hit &&
                static_cast<size_t>(hit) < options.size()) {
                auto &o = options[static_cast<size_t>(hit)];

                if (o.enableEnter && o.type != text_input &&
                    o.type != drag_bar && o.type != deslizant_Select_list) {
                    o.status |= 3;
                }
            }

            captured = -1;
            dragging = false;
            if (mouseCapture == this) {
                mouseCapture = nullptr;
            }
        }

        return true;
    }

    if (focused < 0 || static_cast<size_t>(focused) >= options.size() ||
        !interactive(options[static_cast<size_t>(focused)])) {
        focused = -1;

        for (size_t i = 0; i < options.size(); ++i) {
            if (keyboardFocusable(options[i])) {
                select(static_cast<int>(i));
                break;
            }
        }
    }

    // Escape leaves text editing before considering any command binding.
    if (event.kind == Event::Key && event.action == GLFW_PRESS &&
        event.key == GLFW_KEY_ESCAPE && focused >= 0 &&
        (options[static_cast<size_t>(focused)].status & 4)) {
        options[static_cast<size_t>(focused)].status &= ~4;
        return true;
    }
    if (processCommand(event, true)) {
        return true;
    }

    if (focused < 0) {
        return false;
    }

    auto &o = options[static_cast<size_t>(focused)];

    if (event.kind == Event::Character && o.type == text_input &&
        (o.status & 4)) {
        std::string encoded;

        try {
            utf8::append(event.codepoint, std::back_inserter(encoded));
        } catch (const std::exception &) {
            return true;
        }

        if (o.preText.size() <= o.preTextMaxSize &&
            encoded.size() <= o.preTextMaxSize - o.preText.size()) {
            const auto cursor = CFonts::utf8InsertAt(
                o.preText, encoded,
                static_cast<size_t>(std::max(0, o.strEditPoint)));

            if (cursor != std::string::npos) {
                o.strEditPoint = static_cast<int>(cursor);
            }
        }

        if (o.externalPreTextRef) {
            *o.externalPreTextRef = o.preText;
        }

        return true;
    }

    if (event.kind != Event::Key || event.action == GLFW_RELEASE) {
        return false;
    }

    const int key = event.key;

    if (o.type == text_input && (o.status & 4)) {
        const size_t count = CFonts::utf8Size(o.preText);

        if (count == std::string::npos ||
            count > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return true;
        }

        o.strEditPoint = std::clamp(o.strEditPoint, 0, static_cast<int>(count));

        if (key == GLFW_KEY_LEFT) {
            o.strEditPoint = std::max(0, o.strEditPoint - 1);
        }

        if (key == GLFW_KEY_RIGHT) {
            o.strEditPoint =
                std::min(static_cast<int>(count), o.strEditPoint + 1);
        }

        if (key == GLFW_KEY_HOME) {
            o.strEditPoint = 0;
        }

        if (key == GLFW_KEY_END) {
            o.strEditPoint = static_cast<int>(count);
        }

        if (key == GLFW_KEY_BACKSPACE && o.strEditPoint > 0) {
            CFonts::utf8RemoveAtRange(o.preText, --o.strEditPoint, 1);
        }

        if (key == GLFW_KEY_DELETE) {
            CFonts::utf8RemoveAtRange(o.preText, o.strEditPoint, 1);
        }

        if (key == GLFW_KEY_ENTER) {
            o.status &= ~4;
        }

        if (o.externalPreTextRef) {
            *o.externalPreTextRef = o.preText;
        }

        return true;
    }

    if (o.type == deslizant_Select_list &&
        (key == GLFW_KEY_LEFT || key == GLFW_KEY_RIGHT)) {
        clampList(o);

        if (key == GLFW_KEY_RIGHT &&
            static_cast<size_t>(o.listID) + 1 < o.optList.size()) {
            ++o.listID;
        } else if (key == GLFW_KEY_LEFT && o.listID > 0) {
            --o.listID;
        }

        return true;
    }

    if (key == GLFW_KEY_ENTER && event.action == GLFW_PRESS && o.enableEnter) {
        if (o.type == text_input) {
            o.status |= 4;
        } else {
            o.status |= 3;
        }

        return true;
    }

    const bool previous =
        key == GLFW_KEY_UP ||
        (flow == Flow::Horizontal && key == GLFW_KEY_LEFT) ||
        (key == GLFW_KEY_TAB && (event.mods & GLFW_MOD_SHIFT));
    const bool next = key == GLFW_KEY_DOWN ||
                      (flow == Flow::Horizontal && key == GLFW_KEY_RIGHT) ||
                      (key == GLFW_KEY_TAB && !(event.mods & GLFW_MOD_SHIFT));

    if (previous || next) {
        int i = focused;

        for (size_t n = 0; n < options.size(); ++n) {
            i = (i + (previous ? -1 : 1) + static_cast<int>(options.size())) %
                static_cast<int>(options.size());

            if (keyboardFocusable(options[static_cast<size_t>(i)])) {
                select(i);
                break;
            }
        }
    }
    return previous || next;
}

void CMenu::shortcutCallback(int key, int, int action, int mods) {
    CControls::MenuEvent event;
    event.key = key;
    event.action = action;
    event.mods = mods;
    processEvent(event);
}

void CMenu::dispatchInput() {
    interfaceCalcPos();
    if (!context) {
        CControls::controls().pollMenuDevices();
    }

    auto &pending =
        context ? context->events : CControls::controls().menuEvents;

    if (!containsInterface(inputFocus)) {
        inputFocus = this;
    }
    if (mouseCapture != nullptr && !containsInterface(mouseCapture)) {
        mouseCapture->resetBtns();
    }

    std::vector<CControls::MenuEvent> events;
    events.swap(pending);

    for (const auto &event : events) {
        auto *modalMenu = blockingModal();
        if (modalMenu && !modalMenu->containsInterface(inputFocus)) {
            inputFocus = modalMenu;
        }

        if (event.kind == CControls::MenuEvent::Focus && !event.action) {
            resetBtns();
            visitInterfaces([](CMenu &child) { child.resetBtns(); });
            continue;
        }

        if (event.kind == CControls::MenuEvent::Key &&
            event.key == GLFW_KEY_TAB && event.action != GLFW_RELEASE &&
            focusNextPanel((event.mods & GLFW_MOD_SHIFT) != 0)) {
            continue;
        }

        CMenu *target = inputFocus;
        const bool mouse = event.kind == CControls::MenuEvent::Mouse;
        const bool scroll = event.kind == CControls::MenuEvent::Scroll;
        if (mouse || scroll) {
            const double px = mouse ? event.x : cursorX();
            const double py = mouse ? event.y : cursorY();
            target = modalMenu ? modalMenu : this;

            for (auto it = uiOrderList.rbegin(); it != uiOrderList.rend();
                 ++it) {
                auto *candidate = uiList[static_cast<size_t>(it->id)].m.get();
                if (!containsInterface(candidate) ||
                    (modalMenu && !modalMenu->containsInterface(candidate))) {
                    continue;
                }
                if (candidate->hitRegion(px, py)) {
                    target = candidate;
                    break;
                }
            }

            if (mouse && event.action == GLFW_RELEASE && mouseCapture &&
                containsInterface(mouseCapture)) {
                target = mouseCapture;
            }

            if (mouse && event.action == GLFW_PRESS) {
                focusInterface(*target);
                if (target->uiMenu && !target->permanentPanel) {
                    target->putOnTop();
                }
            }
        }

        const bool navigationKey =
            event.key == GLFW_KEY_UP || event.key == GLFW_KEY_DOWN ||
            event.key == GLFW_KEY_LEFT || event.key == GLFW_KEY_RIGHT ||
            event.key == GLFW_KEY_ENTER || event.key == GLFW_KEY_TAB;
        if (target && !target->processEvent(event) &&
            event.kind == CControls::MenuEvent::Key &&
            event.action == GLFW_PRESS && !navigationKey) {
            // Only permanent panels bubble commands. Stop at a modal or a
            // legacy window; never run ancestor navigation/editing handlers.
            auto *region = target;
            while (region->permanentPanel && region != modalMenu &&
                   region->owner) {
                region = region->owner;
                if (region->processCommand(event, false)) {
                    break;
                }
            }
        }
    }

    events.clear();
    if (pending.empty()) {
        pending.swap(events);
    }
}

void CMenu::update() {
    prepareUpdate();
    dispatchInput();
    finishUpdate();
}

void CMenu::prepareUpdate() {
    if (focused >= static_cast<int>(options.size())) {
        focused = -1;
    }

    for (size_t i = 0; i < options.size(); ++i) {
        auto &o = options[i];
        o.status &= ~3;

        if (static_cast<int>(i) == focused) {
            o.status |= 1;
        }

        clampList(o);
    }

    auto *lua = context ? nullptr : &CLuaH::Lua();

    if (!context && !preEvent) {
        preEvent = lua->idForCallbackEvent("pre" + menuName + "update");
        postEvent = lua->idForCallbackEvent("pos" + menuName + "update");
    }

    if (!context && lua->hasEvent(preEvent)) {
        lua->runEventWithParams(preEvent, {CLuaH::customParam(menuName)});
    }

    const std::string language =
        context ? context->language : GPPGame::GuitarPP().glanguage;
    updateCount = options.size();

    for (size_t i = 0; i < updateCount && i < options.size(); ++i) {
        if (lastLanguage != language) {
            translate(options[i]);
        }

        if (options[i].externalPreTextRef) {
            options[i].preText = *options[i].externalPreTextRef;
        }

        auto callback = options[i].updateCppCallback;

        if (callback) {
            callback(*this, options[i]);
        }
    }

    lastLanguage = language;
    reflow();

    if (focused < 0) {
        for (size_t i = 0; i < options.size(); ++i) {
            if (keyboardFocusable(options[i])) {
                select(static_cast<int>(i));
                break;
            }
        }
    }

    const size_t count = myUiList.size();

    for (size_t i = 0; i < count && i < myUiList.size(); ++i) {
        const int id = myUiList[i];

        if (id >= 0 && static_cast<size_t>(id) < uiList.size() &&
            uiList[static_cast<size_t>(id)].m) {
            uiList[static_cast<size_t>(id)].m->prepareUpdate();
        }
    }
}

void CMenu::finishUpdate() {
    const auto style = resolvedStyle();
    auto *lua = context ? nullptr : &CLuaH::Lua();

    if (dragging && captured >= 0 &&
        static_cast<size_t>(captured) < options.size()) {
        auto &o = options[static_cast<size_t>(captured)];
        const double mx = cursorX(), my = cursorY();

        if (o.type == drag_bar) {
            x += mx - dragX;
            y += my - dragY;
        }

        if (o.type == deslizant_Select_list && !o.optList.empty() &&
            o.deslizantBarSize > 0) {
            auto r = controlRect(static_cast<size_t>(captured), style);
            const double fraction =
                std::clamp((mx - r.left) / o.deslizantBarSize, 0.0, 1.0);
            o.listID = static_cast<int>(std::round(
                fraction * static_cast<double>(o.optList.size() - 1)));
        }

        dragX = mx;
        dragY = my;
    }

    const double mx = cursorX(), my = cursorY();

    hovered = -1;
    if (!isInterfaceOver()) {

        for (size_t i = 0; i < options.size(); ++i) {
            if (interactive(options[i]) && hitOption(i, mx, my, style)) {
                hovered = static_cast<int>(i);

                if ((mx != mouseAX || my != mouseAY) && inputFocus == this &&
                    !(focused >= 0 &&
                      (options[static_cast<size_t>(focused)].status & 4))) {
                    select(hovered);
                }
            }
        }
    }

    mouseAX = mx;
    mouseAY = my;

    for (size_t i = 0; i < updateCount && i < options.size(); ++i) {
        clampList(options[i]);
        auto callback = options[i].posUpdateCppCallback;

        if (callback) {
            callback(*this, options[i]);
        }

        if (i >= options.size()) {
            break;
        }

        const auto &o = options[i];

        if (!context && o.eventID && lua->hasEvent(o.eventID)) {
            CLuaH::multiCallBackParams_t params{
                CLuaH::customParam(menuName), CLuaH::customParam(o.optionName),
                CLuaH::customParam(o.optValue),
                CLuaH::customParam(
                    static_cast<double>(o.status ? o.status : -1))};
            lua->runEventWithParams(o.eventID, params);
        }
    }

    if (!context && lua->hasEvent(postEvent)) {
        lua->runEventWithParams(postEvent, {CLuaH::customParam(menuName)});
    }

    const size_t children = myUiList.size();

    for (size_t i = 0; i < children && i < myUiList.size(); ++i) {
        const int id = myUiList[i];

        if (id < 0 || static_cast<size_t>(id) >= uiList.size() ||
            !uiList[static_cast<size_t>(id)].m) {
            continue;
        }

        auto &child = *uiList[static_cast<size_t>(id)].m;
        child.finishUpdate();
        bool close = false;

        for (const auto &o : child.options) {
            if (o.goback && (o.status & 3) == 3) {
                close = true;
            }
        }

        if (close) {
            uiList[static_cast<size_t>(id)].m.reset();
            myUiList[i] = -1;
        }
    }

    myUiList.erase(std::remove(myUiList.begin(), myUiList.end(), -1),
                   myUiList.end());
}

void CMenu::beginManualEdit(size_t index) {
    if (index >= options.size()) {
        return;
    }

    reflow();
    if (flow != Flow::Manual) {
        for (auto &o : options) {
            if (o.group == flowGroup && !o.manualPosition) {
                // These anchors were overridden by flow. Do not reactivate
                // them when freezing the menu at its current positions.
                o.centerX.reset();
                o.anchorRight = false;
            }
        }
    }

    for (auto &o : options) {
        o.fixedX.reset();
        o.fixedY.reset();
        o.overrideHorizontal = false;
    }
    options[index].centerX.reset();
    options[index].anchorRight = false;
    flow = Flow::Manual;
}

void CMenu::updateDev() {
    const auto style = resolvedStyle();
    const bool down = CEngine::engine().getMouseButton(0) != 0;
    const double mx = cursorX(), my = cursorY();

    if (down && !lastMouseClickStatus && !isInterfaceOver()) {
        for (size_t i = 0; i < options.size(); ++i) {
            if (optionRect(i, style).contains(mx, my)) {
                for (auto &o : options) {
                    o.devStatus = 0;
                }

                captured = static_cast<int>(i);
                options[i].devStatus = 3;
                beginManualEdit(i);
                break;
            }
        }
    }

    if (down && captured >= 0 &&
        static_cast<size_t>(captured) < options.size()) {
        auto &o = options[static_cast<size_t>(captured)];
        o.x += mx - mouseAX;
        o.y += my - mouseAY;
        const auto &engine = CEngine::engine();
        const int direction = engine.getKey(GLFW_KEY_KP_ADD)        ? 1
                              : engine.getKey(GLFW_KEY_KP_SUBTRACT) ? -1
                                                                    : 0;
        if (direction != 0) {
            const auto appearance =
                ui::resolveTypography(style.typography, o.styleOverrides);
            o.styleOverrides.textSize =
                std::max(.005, appearance.textSize +
                                   direction * engine.getDeltaTime() * .1);
        }
    }

    if (!down) {
        captured = -1;

        for (auto &o : options) {
            o.devStatus &= ~1;
        }
    }

    lastMouseClickStatus = down;
    mouseAX = mx;
    mouseAY = my;
}

void CMenu::render() {
    const auto style = resolvedStyle();
    reflow(style);
    auto &engine = CEngine::engine();
    auto &fonts = fontService();
    const std::string defaultFont = "default";
    const auto &fontName = context ? context->font : defaultFont;
    auto color = [&](const ui::Color &c) { glColor4d(c.r, c.g, c.b, c.a); };

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT |
                 GL_SCISSOR_BIT | GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    GLint program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    engine.invalidateRenderBindings();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GLint matrixMode, viewportPixels[4];
    glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
    glGetIntegerv(GL_VIEWPORT, viewportPixels);
    const double menuAspect =
        viewportPixels[3] > 0
            ? static_cast<double>(viewportPixels[2]) / viewportPixels[3]
            : 1;
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-menuAspect, menuAspect, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    const double aspect = vp[3] > 0 ? static_cast<double>(vp[2]) / vp[3] : 1;
    const bool hadScissor = glIsEnabled(GL_SCISSOR_TEST) != 0;
    GLint previousScissor[4];
    glGetIntegerv(GL_SCISSOR_BOX, previousScissor);
    const auto panelBounds = getPanelBounds();
    const Rect menuClip = permanentPanel
                              ? Rect{std::max(-aspect, panelBounds.left + x),
                                     std::max(-1.0, panelBounds.bottom + y),
                                     std::min(aspect, panelBounds.right + x),
                                     std::min(1.0, panelBounds.top + y)}
                              : Rect{-aspect, -1, aspect, 1};
    Rect activeClip = menuClip;
    auto clip = [&](Rect r) {
        r.left = std::max(r.left, activeClip.left);
        r.right = std::min(r.right, activeClip.right);
        r.bottom = std::max(r.bottom, activeClip.bottom);
        r.top = std::min(r.top, activeClip.top);
        const int left =
            vp[0] + static_cast<int>((r.left / aspect + 1) * vp[2] / 2);
        const int bottom = vp[1] + static_cast<int>((r.bottom + 1) * vp[3] / 2);
        const int right =
            vp[0] + static_cast<int>((r.right / aspect + 1) * vp[2] / 2);
        const int top = vp[1] + static_cast<int>((r.top + 1) * vp[3] / 2);
        const int l = hadScissor ? std::max(left, previousScissor[0]) : left;
        const int b =
            hadScissor ? std::max(bottom, previousScissor[1]) : bottom;
        const int rr =
            hadScissor
                ? std::min(right, previousScissor[0] + previousScissor[2])
                : right;
        const int tt =
            hadScissor ? std::min(top, previousScissor[1] + previousScissor[3])
                       : top;
        glEnable(GL_SCISSOR_TEST);
        glScissor(l, b, std::max(0, rr - l), std::max(0, tt - b));
    };

    if (permanentPanel || hadScissor) {
        clip(menuClip);
    }
    if (qbgd.Text) {
        color(style.panel.background);
        rectangle({qbgd.x1 + x, qbgd.y3 + y, qbgd.x2 + x, qbgd.y1 + y},
                  qbgd.Text);
    }

    auto border = [&](const Rect &rect, const ui::Color &tint, double width) {
        if (!std::isfinite(width) || width <= 0) {
            return;
        }
        const double w = std::min({width, (rect.right - rect.left) / 2,
                                   (rect.top - rect.bottom) / 2});
        color(tint);
        rectangle({rect.left, rect.bottom, rect.right, rect.bottom + w});
        rectangle({rect.left, rect.top - w, rect.right, rect.top});
        rectangle({rect.left, rect.bottom + w, rect.left + w, rect.top - w});
        rectangle({rect.right - w, rect.bottom + w, rect.right, rect.top - w});
    };

    unsigned pickTexture = context ? context->pickTexture : 0;
    if (!context) {
        const auto &sprites = GPPGame::GuitarPP().SPR;
        auto pick = sprites.find("palheta");
        pickTexture = pick == sprites.end() ? 0 : pick->second;
    }

    for (size_t i = 0; i < options.size(); ++i) {
        auto &o = options[i];
        ensureLayout(o);
        const auto appearance =
            ui::resolveTypography(style.typography, o.styleOverrides);
        const Rect r = optionRect(i, style);
        const bool clipped = viewportEnabled && o.group == flowGroup;
        const Rect view{viewport.left + x, viewport.bottom + y,
                        viewport.right + x, viewport.top + y};

        if (r.right <= r.left || r.top <= r.bottom || !r.intersects(menuClip) ||
            (clipped && !r.intersects(view))) {
            continue;
        }

        activeClip = menuClip;
        if (clipped) {
            activeClip = {std::max(menuClip.left, view.left),
                          std::max(menuClip.bottom, view.bottom),
                          std::min(menuClip.right, view.right),
                          std::min(menuClip.top, view.top)};
        }

        if (clipped || hadScissor || permanentPanel) {
            clip(activeClip);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }

        const double px = o.x + x, py = o.y + y + (clipped ? scrollOffset : 0);

        if (o.type == panel || o.styleOverrides.background) {
            color(o.styleOverrides.background.value_or(style.panel.background));
            rectangle(r);
        }
        const bool ownsFocus = inputFocus == this || (!inputFocus && !uiMenu);
        const bool focusHere =
            ownsFocus && static_cast<int>(i) == focused && interactive(o);
        if (o.active || focusHere) {
            auto selectedColor = style.selection.background;

            if (style.selection.animated) {
                selectedColor.a *= .9 + .1 * std::sin(engine.getTime() * 4);
            }

            color(selectedColor);
            rectangle(r);
        }
        if (focusHere &&
            (o.type == table_row ||
             (o.type == textbtn && (o.width > 0 || o.height > 0)))) {
            border(r, style.selection.accent,
                   std::max(.004, 2.0 / std::max(1, vp[3])));
        } else if (focusHere) {
            color(style.selection.accent);
            rectangle(
                {r.left - appearance.textSize * (1 + style.selection.pickGap),
                 py, r.left - appearance.textSize * style.selection.pickGap,
                 py + appearance.textSize},
                pickTexture);
        }

        glColor4d(appearance.textColor.r, appearance.textColor.g,
                  appearance.textColor.b,
                  o.disabled ? .35 : appearance.textColor.a);

        if (o.type == panel) {
            border(
                r, o.styleOverrides.border.value_or(style.panel.border),
                o.styleOverrides.borderWidth.value_or(style.panel.borderWidth));
        } else if (o.type == image) {
            if (o.texture) {
                glColor4d(1, 1, 1, o.disabled ? .35 : 1);
                rectangle(r, o.texture);
            }
        } else if (o.type == table_row) {
            const size_t count = std::min(o.cells.size(), tableColumns.size());
            o.cellLayouts.resize(count);
            double left = r.left;
            const double pad = style.layout.padding * appearance.textSize;
            for (size_t cell = 0; cell < count; ++cell) {
                const double width = tableColumns[cell];
                if (!std::isfinite(width) || width <= 0) {
                    continue;
                }
                const Rect cellRect{left, r.bottom, left + width, r.top};
                left += width;
                if (!cellRect.intersects(activeClip)) {
                    continue;
                }
                auto &cache = o.cellLayouts[cell];
                if (cache.revision != fonts.revision() ||
                    cache.text != o.cells[cell] || cache.font != fontName) {
                    fonts.layoutText(o.cells[cell], cache.layout, fontName);
                    cache.font = fontName;
                    cache.text = o.cells[cell];
                    cache.revision = fonts.revision();
                }
                clip({cellRect.left + pad, r.bottom, cellRect.right - pad,
                      r.top});
                const double textY = (r.bottom + r.top -
                                      (cache.layout.top + cache.layout.bottom) *
                                          appearance.textSize) /
                                     2;
                glEnable(GL_TEXTURE_2D);
                fonts.drawLayout(cache.layout,
                                 cellRect.left + pad -
                                     (cache.layout.left * appearance.textSize),
                                 textY, appearance.textSize);
            }
        } else if (o.type == text_input) {
            color(style.controls.field);
            rectangle(r);
            clip({px, py, px + o.fieldWidth, py + appearance.textSize});
            const size_t cursor =
                std::min(static_cast<size_t>(std::max(0, o.strEditPoint)),
                         o.layout.carets.empty() ? size_t(0)
                                                 : o.layout.carets.size() - 1);
            const double caret =
                o.layout.carets.empty()
                    ? 0
                    : o.layout.carets[cursor] * appearance.textSize;
            o.textScroll =
                std::clamp(o.textScroll,
                           std::max(0.0, caret - o.fieldWidth +
                                             std::min(.02, o.fieldWidth * .25)),
                           std::max(0.0, caret));
            glColor4d(appearance.textColor.r, appearance.textColor.g,
                      appearance.textColor.b, appearance.textColor.a);
            glEnable(GL_TEXTURE_2D);
            fonts.drawLayout(o.layout, px - o.textScroll, py,
                             appearance.textSize);

            if (o.status & 4) {
                color(style.selection.accent);
                rectangle({px + caret - o.textScroll, py,
                           px + caret - o.textScroll + .003,
                           py + appearance.textSize});
            }
        } else if (o.type == drag_bar) {
            color(style.controls.track);
            rectangle(controlRect(i, style));
        } else {
            if (o.type == textbtn && (o.width > 0 || o.height > 0)) {
                clip(r);
            }
            glEnable(GL_TEXTURE_2D);
            fonts.drawLayout(o.layout, px, py, appearance.textSize);

            if (o.type == deslizant_Select_list) {
                const auto bar = controlRect(i, style);
                color(style.controls.track);
                rectangle(bar);

                if (!o.optList.empty() && o.deslizantBarSize > 0) {
                    const double fraction =
                        o.optList.size() > 1
                            ? static_cast<double>(o.listID) /
                                  static_cast<double>(o.optList.size() - 1)
                            : 0;
                    const double cursor =
                        bar.left +
                        fraction * std::max(0.0, o.deslizantBarSize -
                                                     appearance.textSize);
                    color(style.selection.accent);
                    rectangle({cursor, bar.bottom, cursor + appearance.textSize,
                               bar.top},
                              pickTexture);
                    glColor4d(appearance.textColor.r, appearance.textColor.g,
                              appearance.textColor.b, appearance.textColor.a);
                    glEnable(GL_TEXTURE_2D);
                    fonts.drawLayout(o.valueLayout,
                                     bar.right + appearance.textSize, py,
                                     appearance.textSize);
                }
            }
        }
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(matrixMode);
    glPopClientAttrib();
    glPopAttrib();
    glUseProgram(static_cast<GLuint>(program));
    engine.invalidateRenderBindings();
}

int CMenu::getUIListSize() { return static_cast<int>(myUiList.size()); }

CMenu::uiWindowStruct &CMenu::getUILast() {
    static uiWindowStruct empty;
    return uiList.empty() ? empty : uiList.back();
}

void CMenu::interfaceCalcPos() {
    uiOrderList.clear();

    for (size_t i = 0; i < uiList.size(); ++i) {
        if (uiList[i].m) {
            uiOrderList.push_back({static_cast<int>(i), uiList[i].pos});
        }
    }

    // Root panels sit below windows. Panels inside a window follow that
    // window's layer; modals and their descendants remain above other windows.
    auto layer = [](const posUiOrder &entry) {
        const auto *menu = uiList[static_cast<size_t>(entry.id)].m.get();
        const CMenu *window = menu;
        while (window && window->permanentPanel) {
            window = window->owner;
        }
        const int z = window && window->uiMenu
                          ? uiList[static_cast<size_t>(window->thisUiID)].pos
                          : 0;
        bool inModal = false;
        for (const auto *ancestor = menu; ancestor != nullptr;
             ancestor = ancestor->owner) {
            inModal = inModal || ancestor->modal;
        }
        return std::make_tuple(inModal, z, entry.pos);
    };
    std::stable_sort(uiOrderList.begin(), uiOrderList.end(),
                     [&](const posUiOrder &a, const posUiOrder &b) {
                         return layer(a) < layer(b);
                     });
}

void CMenu::refreshMenusUiPosOrder() { interfaceCalcPos(); }

CMenu *CMenu::getUiMenuOnTop() {
    interfaceCalcPos();
    return uiOrderList.empty()
               ? nullptr
               : uiList[static_cast<size_t>(uiOrderList.back().id)].m.get();
}

CMenu::posUiOrder &CMenu::getThisOnUiOrder() const {
    for (auto &p : uiOrderList) {
        if (p.id == thisUiID) {
            return p;
        }
    }

    throw std::logic_error("Interface is not registered");
}

CMenu::posUiOrder &CMenu::getTopUI() {
    if (uiOrderList.empty()) {
        throw std::logic_error("No interface");
    }

    return uiOrderList.back();
}

void CMenu::putOnTop() {
    interfaceCalcPos();
    int pos = 0;

    for (const auto &p : uiOrderList) {
        if (p.id != thisUiID) {
            uiList[static_cast<size_t>(p.id)].pos = ++pos;
        }
    }

    if (thisUiID >= 0) {
        uiList[static_cast<size_t>(thisUiID)].pos = ++pos;
    }

    interfaceCalcPos();
}

bool CMenu::isThisOnTop() const { return getUiMenuOnTop() == this; }

int CMenu::allocOrGetUiFreeSpace() {
    for (size_t i = 0; i < uiList.size(); ++i) {
        if (!uiList[i].m) {
            return static_cast<int>(i);
        }
    }

    uiList.emplace_back();
    return static_cast<int>(uiList.size() - 1);
}

int CMenu::attachInterface(const CMenu &m, bool asPanel,
                           std::optional<Rect> bounds) {
    const int id = allocOrGetUiFreeSpace();
    auto instance = std::make_unique<CMenu>(m);
    instance->uiMenu = true;
    instance->thisUiID = id;
    instance->owner = this;
    instance->permanentPanel = asPanel;
    instance->setPanelBounds(bounds);
    if (instance->presentationBase) {
        instance->presentationBase->panelBounds = bounds;
    }

    instance->returnFocus = rootMenu()->focusedInterface();
    if (asPanel) {
        instance->modal = false;
    }

    uiList[static_cast<size_t>(id)].m = std::move(instance);
    myUiList.push_back(id);

    auto &child = *uiList[static_cast<size_t>(id)].m;
    child.putOnTop();

    if (!asPanel) {
        if (child.modal) {
            rootMenu()->resetBtns();
            rootMenu()->visitInterfaces([](CMenu &menu) { menu.resetBtns(); });
        }

        auto *blocker = blockingModal();
        if (!blocker || blocker->containsInterface(&child)) {
            inputFocus = &child;
        }
    }

    return id;
}

int CMenu::pushUserInterface(const CMenu &m) {
    return attachInterface(m, false, {});
}

CMenu::Rect CMenu::getPanelBounds() const {
    return fixedPanelBounds.value_or(viewport);
}

std::optional<CMenu::Rect> CMenu::getFixedPanelBounds() const {
    return fixedPanelBounds;
}

void CMenu::setPanelBounds(std::optional<Rect> bounds) {
    if (bounds &&
        (!std::isfinite(bounds->left) || !std::isfinite(bounds->right) ||
         !std::isfinite(bounds->top) || !std::isfinite(bounds->bottom) ||
         bounds->left >= bounds->right || bounds->bottom >= bounds->top)) {
        throw std::invalid_argument("Invalid panel bounds");
    }
    fixedPanelBounds = bounds;
}

int CMenu::pushPanel(const CMenu &m, Rect bounds) {
    return attachInterface(m, true, bounds);
}

int CMenu::pushPanel(const CMenu &m) {
    return attachInterface(m, true, std::nullopt);
}

void CMenu::renderUiList() {
    interfaceCalcPos();

    for (const auto &p : uiOrderList) {
        uiList[static_cast<size_t>(p.id)].m->render();
    }
}

size_t CMenu::getDevSelectedMenuOpt() {
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i].devStatus & 2) {
            return i;
        }
    }

    return ~size_t(0);
}

CFonts &CMenu::fontService() const {
    return context && context->fonts ? *context->fonts : CFonts::fonts();
}

double CMenu::aspect() const {
    return context ? context->aspect : GPPGame::getWindowProportion();
}

double CMenu::cursorX() const {
    return context ? context->mouseX : CEngine::engine().mouseX;
}

double CMenu::cursorY() const {
    return context ? context->mouseY : CEngine::engine().mouseY;
}

void CMenu::translate(menuOpt &option) {
    if (!context) {
        option.update();
    } else if (context->translate && !option.langEntryKey.empty()) {
        const auto text = context->translate(option.langEntryKey);
        if (!text.empty()) {
            option.text = text;
        }
    }
}

CMenu::menuOpt *CMenu::findOption(const std::string &id) {
    const std::string qualified =
        id.compare(0, menuName.size() + 1, menuName + ".") == 0
            ? id
            : menuName + "." + id;
    for (auto &option : options) {
        if (option.optionName == qualified) {
            return &option;
        }
    }
    return nullptr;
}

void CMenu::applyFixedPositions(const ui::Style &style) {
    for (auto &o : options) {
        if (o.fixedX) {
            o.x = *o.fixedX;
        }
        if (o.fixedY) {
            o.y = *o.fixedY;
        }
        if (o.overrideHorizontal) {
            const auto r = layoutRect(o, style);
            if (o.anchorRight) {
                o.x = aspect() - .1 - r.right;
            } else if (o.centerX) {
                o.x = *o.centerX - (r.left + r.right) / 2;
            }
        }
    }
}

void CMenu::clearOptions() {
    if (presentationBase) {
        ui::restorePresentation(*this, *presentationBase);
    }
    presentationBase.reset();
    presentationPatch.reset();
    closeInterfaces();
    options.clear();
    groupInfo.clear();
    resetData();
    scrollOffset = 0;
}

void CMenu::applyPresentationOverrides(const ui::MenuOverrides &patch) {
    ui::validateOverrides({{menuName, patch}});

    if (!presentationBase) {
        presentationBase = std::make_unique<ui::MenuPresentation>(
            ui::capturePresentation(*this));
    }

    ui::restorePresentation(*this, *presentationBase);
    presentationPatch = std::make_unique<ui::MenuOverrides>(patch);
    ui::applyMenu(*this, patch);
}

const ui::MenuPresentation *CMenu::originalPresentation() const {
    return presentationBase.get();
}

void CMenu::visitInterfaces(const std::function<void(CMenu &)> &visitor) {
    for (int id : myUiList) {
        if (id >= 0 && static_cast<size_t>(id) < uiList.size() &&
            uiList[static_cast<size_t>(id)].m) {
            auto &child = *uiList[static_cast<size_t>(id)].m;
            visitor(child);
            child.visitInterfaces(visitor);
        }
    }
}
