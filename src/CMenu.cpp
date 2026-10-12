#include "CMenu.h"
#include "CLanguageManager.h"
#include "GPPGame.h"
#include "utf8.h"
#include <GL/glew.h>
#include <algorithm>
#include <cmath>
#include <limits>

std::deque<CMenu::uiWindowStruct> CMenu::uiList;
std::vector<CMenu::posUiOrder> CMenu::uiOrderList;
int CMenu::menusCreated = 0;
CMenu *CMenu::inputFocus = nullptr;

namespace {
void rectangle(const CMenu::Rect &r, unsigned texture = 0) {
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
    return !o.disabled && o.type != CMenu::static_text && o.type != CMenu::none;
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
    x = m.x;
    y = m.y;
    styleOverrides = m.styleOverrides;
    flow = m.flow;
    modal = m.modal;
    viewportEnabled = m.viewportEnabled;
    viewport = m.viewport;
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
    closeInterfaces();

    if (inputFocus == this) {
        inputFocus = nullptr;
    }
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

    if (inputFocus == this) {
        inputFocus = nullptr;
    }
}

int CMenu::addOpt(const menuOpt &opt) {
    const size_t index = options.size();

    if (index >= static_cast<size_t>(std::numeric_limits<int>::max())) {
        return -1;
    }

    options.push_back(opt);
    auto &o = options.back();
    translate(o);
    o.optionName =
        menuName + "_" + std::to_string(index) + std::to_string(o.group);
    o.eventID = context ? 0 : CLuaH::Lua().idForCallbackEvent(o.optionName);
    groupInfo.try_emplace(o.group);
    return static_cast<int>(index);
}

void CMenu::resetData() {
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

CMenu::Rect CMenu::optionRect(size_t i, const ui::Style &style) {
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
    Rect r{o.x + x + o.layout.left * appearance.textSize,
           yy + o.layout.bottom * appearance.textSize,
           o.x + x + o.layout.right * appearance.textSize,
           yy + o.layout.top * appearance.textSize};

    if (o.type == text_input) {
        r = {o.x + x, yy, o.x + x + o.fieldWidth, yy + appearance.textSize};
    }

    if (o.type == drag_bar) {
        r = controlRect(i, style);
    }

    if (o.type == deslizant_Select_list) {
        r.right = controlRect(i, style).right + appearance.textSize +
                  o.valueLayout.width() * appearance.textSize;
    }

    const double pad = style.layout.padding * appearance.textSize;
    return {r.left - pad, r.bottom - pad, r.right + pad, r.top + pad};
}

void CMenu::setFlow(Flow direction, double originX, double originY, int group) {
    flow = direction;
    flowX = originX;
    flowY = originY;
    flowGroup = group;
}

void CMenu::reflow(const ui::Style &style) {
    for (auto &o : options) {
        const auto appearance =
            ui::resolveTypography(style.typography, o.styleOverrides);
        if (o.centerX && !o.anchorRight &&
            (flow == Flow::Manual || o.group != flowGroup ||
             o.manualPosition)) {
            ensureLayout(o);
            o.x = *o.centerX -
                  (o.layout.left + o.layout.right) * appearance.textSize / 2;
        }

        if (o.anchorRight) {
            ensureLayout(o);
            o.x = aspect() - .1 - o.layout.right * appearance.textSize;
        }
    }

    if (flow == Flow::Manual) {
        refreshFocusGeometry(style);
        return;
    }

    if (anchorLeft) {
        flowX = -aspect() + leftMargin;
    }

    double px = flowX, py = flowY;

    for (auto &o : options) {
        if (o.group == flowGroup && !o.manualPosition) {
            ensureLayout(o);
            const auto appearance =
                ui::resolveTypography(style.typography, o.styleOverrides);
            o.x = px;
            o.y = py;

            if (flow == Flow::Vertical) {
                py -= std::max(style.layout.rowStep,
                               (o.layout.top - o.layout.bottom + .3) *
                                   appearance.textSize);
            } else {
                px +=
                    o.layout.width() * appearance.textSize +
                    (1 + style.selection.pickGap + .5) * appearance.textSize;
            }
        }
    }

    refreshFocusGeometry(style);
}

void CMenu::refreshFocusGeometry(const ui::Style &style) {
    if (!viewportEnabled || focused < 0 ||
        static_cast<size_t>(focused) >= options.size() ||
        options[static_cast<size_t>(focused)].group != flowGroup) {
        focusGeometryOption = -1;
        return;
    }

    auto &o = options[static_cast<size_t>(focused)];
    ensureLayout(o);
    const auto appearance =
        ui::resolveTypography(style.typography, o.styleOverrides);
    // Exclude scrollOffset: scrolling must not pull focus back into view.
    const std::array<double, 5> geometry{o.y, appearance.textSize,
                                         style.layout.padding, o.layout.top,
                                         o.layout.bottom};
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

bool CMenu::isMouseOnThisMenu() {
    const auto style = resolvedStyle();
    const double mx = cursorX(), my = cursorY();

    if (qbgd.Text &&
        Rect{qbgd.x1 + x, qbgd.y3 + y, qbgd.x2 + x, qbgd.y1 + y}.contains(mx,
                                                                          my)) {
        return true;
    }

    for (size_t i = 0; i < options.size(); ++i) {
        if (optionRect(i, style).contains(mx, my)) {
            return true;
        }
    }

    return false;
}

bool CMenu::isInterfaceOver() const {
    for (auto it = uiOrderList.rbegin(); it != uiOrderList.rend(); ++it) {
        auto *m = uiList[static_cast<size_t>(it->id)].m.get();

        if (m == this) {
            break;
        }

        if (m && (m->modal || m->isMouseOnThisMenu())) {
            return true;
        }
    }

    return false;
}

void CMenu::processEvent(const CControls::MenuEvent &event) {
    const auto style = resolvedStyle();
    reflow(style);
    using Event = CControls::MenuEvent;

    if (event.kind == Event::Focus && !event.action) {
        resetBtns();
        return;
    }

    if (event.kind == Event::Scroll && viewportEnabled) {
        double lowest = viewport.top;

        for (const auto &o : options) {
            if (o.group == flowGroup) {
                lowest = std::min(lowest, o.y);
            }
        }

        scrollOffset =
            std::clamp(scrollOffset - event.y * style.layout.rowStep * 3, 0.0,
                       std::max(0.0, viewport.bottom - lowest));
        return;
    }

    if (event.kind == Event::Mouse && event.key == GLFW_MOUSE_BUTTON_LEFT) {
        int hit = -1;

        for (size_t i = 0; i < options.size(); ++i) {
            if (viewportEnabled && options[i].group == flowGroup &&
                !Rect{viewport.left + x, viewport.bottom + y,
                      viewport.right + x, viewport.top + y}
                     .contains(event.x, event.y)) {
                continue;
            }

            if (interactive(options[i]) &&
                optionRect(i, style).contains(event.x, event.y)) {
                hit = static_cast<int>(i);
            }
        }

        if (event.action == GLFW_PRESS) {
            captured = hit;

            if (hit < 0) {
                return;
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
                o.strEditPoint = static_cast<int>(
                    o.layout.cursorAt((event.x - x - o.x + o.textScroll) /
                                      appearance.textSize));
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
        }

        return;
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

    if (event.kind == Event::Key && event.action == GLFW_PRESS) {
        for (size_t i = 0; i < options.size(); ++i) {
            if (interactive(options[i]) && options[i].shortcutKey &&
                options[i].shortcutKey == event.key) {
                select(static_cast<int>(i));
                options[i].status |= 3;
                return;
            }
        }

        if (event.key == GLFW_KEY_ESCAPE) {
            if (focused >= 0 &&
                (options[static_cast<size_t>(focused)].status & 4)) {
                options[static_cast<size_t>(focused)].status &= ~4;
                return;
            }

            for (size_t i = 0; i < options.size(); ++i) {
                if (options[i].goback && interactive(options[i])) {
                    select(static_cast<int>(i));
                    options[i].status |= 3;
                    return;
                }
            }
        }
    }

    if (focused < 0) {
        return;
    }

    auto &o = options[static_cast<size_t>(focused)];

    if (event.kind == Event::Character && o.type == text_input &&
        (o.status & 4)) {
        std::string encoded;

        try {
            utf8::append(event.codepoint, std::back_inserter(encoded));
        } catch (const std::exception &) {
            return;
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

        return;
    }

    if (event.kind != Event::Key || event.action == GLFW_RELEASE) {
        return;
    }

    const int key = event.key;

    if (o.type == text_input && (o.status & 4)) {
        const size_t count = CFonts::utf8Size(o.preText);

        if (count == std::string::npos ||
            count > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return;
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

        return;
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

        return;
    }

    if (key == GLFW_KEY_ENTER && event.action == GLFW_PRESS && o.enableEnter) {
        if (o.type == text_input) {
            o.status |= 4;
        } else {
            o.status |= 3;
        }

        return;
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
    bool valid = inputFocus == this;

    for (auto &slot : uiList) {
        if (slot.m && slot.m.get() == inputFocus) {
            valid = true;
        }
    }

    if (!valid) {
        inputFocus = this;
    }

    CMenu *top = getUiMenuOnTop();

    if (top && top->modal) {
        inputFocus = top;
    }

    // Reuse capacity, and isolate this dispatch from callbacks appending
    // events.
    std::vector<CControls::MenuEvent> events;
    events.swap(pending);

    for (const auto &event : events) {
        CMenu *target = inputFocus;

        if (event.kind == CControls::MenuEvent::Mouse) {
            target = this;

            for (auto it = uiOrderList.rbegin(); it != uiOrderList.rend();
                 ++it) {
                auto *m = uiList[static_cast<size_t>(it->id)].m.get();

                if (!m) {
                    continue;
                }

                const auto childStyle = m->resolvedStyle();
                bool hit = m->modal;

                for (size_t i = 0; !hit && i < m->options.size(); ++i) {
                    hit = m->optionRect(i, childStyle)
                              .contains(event.x, event.y);
                }

                if (hit) {
                    target = m;
                    break;
                }
            }

            // Mouse release belongs to the menu that captured the press.
            if (event.action == GLFW_RELEASE && inputFocus &&
                inputFocus->captured >= 0) {
                target = inputFocus;
            }

            if (event.action == GLFW_PRESS) {
                inputFocus = target;

                if (target->uiMenu) {
                    target->putOnTop();
                }
            }
        }

        if (event.kind == CControls::MenuEvent::Focus && !event.action) {
            resetBtns();

            for (auto &slot : uiList) {
                if (slot.m) {
                    slot.m->resetBtns();
                }
            }
        } else if (target) {
            target->processEvent(event);
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

    if (!isInterfaceOver() && (mx != mouseAX || my != mouseAY)) {
        hovered = -1;

        for (size_t i = 0; i < options.size(); ++i) {
            if (interactive(options[i]) &&
                optionRect(i, style).contains(mx, my)) {
                if (viewportEnabled && options[i].group == flowGroup &&
                    !Rect{viewport.left + x, viewport.bottom + y,
                          viewport.right + x, viewport.top + y}
                         .contains(mx, my)) {
                    continue;
                }

                hovered = static_cast<int>(i);

                if (inputFocus == this &&
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
            if (inputFocus == &child) {
                inputFocus = this;
            }

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
    auto color = [&](const ui::Color &c) {
        engine.setColor(c.r, c.g, c.b, c.a);
    };

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT |
                 GL_SCISSOR_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
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

    if (qbgd.Text) {
        color(style.panel.background);
        rectangle({qbgd.x1 + x, qbgd.y3 + y, qbgd.x2 + x, qbgd.y1 + y},
                  qbgd.Text);
    }

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    const double aspect = vp[3] > 0 ? static_cast<double>(vp[2]) / vp[3] : 1;
    const bool hadScissor = glIsEnabled(GL_SCISSOR_TEST) != 0;
    GLint previousScissor[4];
    glGetIntegerv(GL_SCISSOR_BOX, previousScissor);
    Rect activeClip{-aspect, -1, aspect, 1};
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

        if (clipped && !r.intersects(view)) {
            continue;
        }

        activeClip = clipped ? view : Rect{-aspect, -1, aspect, 1};

        if (clipped || hadScissor) {
            clip(activeClip);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }

        const double px = o.x + x, py = o.y + y + (clipped ? scrollOffset : 0);

        if (static_cast<int>(i) == focused) {
            auto selectedColor = style.selection.background;

            if (style.selection.animated) {
                selectedColor.a *= .9 + .1 * std::sin(engine.getTime() * 4);
            }

            color(selectedColor);
            rectangle(r);
            color(style.selection.accent);
            rectangle(
                {r.left - appearance.textSize * (1 + style.selection.pickGap),
                 py, r.left - appearance.textSize * style.selection.pickGap,
                 py + appearance.textSize},
                pickTexture);
        }

        engine.setColor(appearance.textColor.r, appearance.textColor.g,
                        appearance.textColor.b,
                        o.disabled ? .35 : appearance.textColor.a);

        if (o.type == text_input) {
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
            engine.setColor(appearance.textColor.r, appearance.textColor.g,
                            appearance.textColor.b, appearance.textColor.a);
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
                    rectangle({cursor, bar.bottom,
                               cursor + appearance.textSize, bar.top},
                              pickTexture);
                    engine.setColor(
                        appearance.textColor.r, appearance.textColor.g,
                        appearance.textColor.b, appearance.textColor.a);
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
    glPopAttrib();
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

    std::stable_sort(uiOrderList.begin(), uiOrderList.end());
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

int CMenu::pushUserInterface(const CMenu &m) {
    const int id = allocOrGetUiFreeSpace();
    auto instance = std::make_unique<CMenu>(m);
    instance->uiMenu = true;
    instance->thisUiID = id;
    uiList[static_cast<size_t>(id)].m = std::move(instance);
    myUiList.push_back(id);
    uiList[static_cast<size_t>(id)].m->putOnTop();
    inputFocus = uiList[static_cast<size_t>(id)].m.get();
    return id;
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
