#pragma once
#include <cstddef>
#ifndef GUITARPP_CMENU_H
#define GUITARPP_CMENU_H

#include "CControls.h"
#include "CUIStyle.h"
#include "CEngine.h"
#include "CFonts.h"
#include "CLuaH.hpp"
#include <array>
#include <cstdint>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <string>

class CMenu {
    std::string menuName;
    static int menusCreated;
    // static std::map <std::string, CMenu*> Menus;

    static std::string textBuffer;

  public:
    struct uiWindowStruct {
        std::unique_ptr<CMenu> m;
        int pos;

        uiWindowStruct() { pos = 0; }
    };

    struct posUiOrder {
        int id;
        int pos;

        bool operator>(const posUiOrder &p0) const { return pos > p0.pos; }

        bool operator<(const posUiOrder &p0) const { return pos < p0.pos; }

        bool operator==(const posUiOrder &p0) const { return pos == p0.pos; }
    };

  private:
    static std::deque<uiWindowStruct> uiList;
    static std::vector<posUiOrder> uiOrderList;
    std::deque<int> myUiList;

    bool uiMenu;
    int thisUiID;

    int focused = -1, hovered = -1, captured = -1;
    int focusGeometryOption = -1;
    std::array<double, 5> focusGeometry{};
    bool dragging = false;
    double dragX = 0, dragY = 0;
    static CMenu *inputFocus;
    std::string lastLanguage;
    int preEvent = 0, postEvent = 0;
    void dispatchInput();
    void prepareUpdate();
    void finishUpdate();
    size_t updateCount = 0;
    bool lastEnterOptBtn;
    bool lastMouseClickStatus{};

    static int allocOrGetUiFreeSpace();

    static void refreshMenusUiPosOrder();
    static void interfaceCalcPos();

    posUiOrder &getThisOnUiOrder() const;
    static posUiOrder &getTopUI();
    void putOnTop();
    bool isThisOnTop() const;

  public:
    // Owned by the host and used only on the UI/context thread. Null keeps
    // the game's existing services. Preview never calls game/Lua callbacks.
    struct Context {
        CFonts *fonts = nullptr;
        std::string font = "default", language = "PT-BR";
        ui::Style style;
        double aspect = 1, mouseX = 0, mouseY = 0;
        unsigned pickTexture = 0;
        std::vector<CControls::MenuEvent> events;
        std::function<std::string(const std::string &)> translate;
    };

    struct Rect {
        double left = 0, bottom = 0, right = 0, top = 0;
        bool contains(double px, double py) const {
            return px >= left && px <= right && py >= bottom && py <= top;
        }

        bool intersects(const Rect &r) const {
            return right >= r.left && left <= r.right && top >= r.bottom &&
                   bottom <= r.top;
        }
    };

    ui::StyleOverrides styleOverrides;
    ui::Style resolvedStyle() const;
    enum class Flow { Manual, Vertical, Horizontal };
    Flow flow = Flow::Manual;
    bool modal = false;
    bool viewportEnabled = false;
    Rect viewport{-1, -.75, 1, .75};
    double scrollOffset = 0, flowX = 0, flowY = .4;
    int flowGroup = 1;
    bool anchorLeft = false;
    double leftMargin = .2;
    void setFlow(Flow direction, double originX, double originY, int group = 1);
    void reflow();
    void processEvent(const CControls::MenuEvent &);
    int focusedOption() const { return focused; }

    void closeInterfaces();
    enum menusOPT {
        none = -1,
        button_ok,
        text_input,
        button_cancel,
        textbtn,
        multi_btn,
        deslizant_Select_list,
        button_ui,
        drag_bar,
        static_text
    };

    static CMenu *getUiMenuOnTop();
    bool isInterfaceOver() const;
    int getUIListSize();
    bool isMouseOnThisMenu();

    static inline uiWindowStruct &getUiAt(size_t index) {
        return uiList.at(index);
    }

    static void renderUiList();
    static uiWindowStruct &getUILast();

    double x, y;
    bool devMenuNOUpdateOthers;
    std::string mTitle;

    CLuaH::luaScript *lScript;
    std::map<std::string, int> luaCallBacks;

    CEngine::RenderDoubleStruct qbgd{};

    std::string backgroundTexture;
    CMenu *devEditMenu;
    int devEditingOpt;

    bool temp, gameMenu;

    double mouseAX;
    double mouseAY;

    struct menuOpt {
        bool disabled = false;
        bool manualPosition = false;
        bool anchorRight = false;
        std::optional<double> centerX;
        ui::OptionStyleOverrides styleOverrides;
        double fieldWidth = .55, textScroll = 0;
        int eventID = 0;
        CFonts::TextLayout layout, valueLayout;
        std::string cachedText, cachedValue;
        size_t cachedRevision = ~size_t(0), valueRevision = ~size_t(0);
        menusOPT type;
        int group;
        unsigned int texture;
        std::string optionName;

        bool goback;

        bool enableEnter;

        int pressingShortcutKey;
        int shortcutKey;

        std::string text, preText, langEntryKey;
        std::string *externalPreTextRef;
        size_t preTextMaxSize;
        int status;
        int devStatus;
        double optValue, deslizantBarSize;
        int listID;

        bool btnClickStat;

        CLuaH::luaScript *attachedLua;

        std::deque<std::string> optList;

        std::deque<std::string> menusXRef;

        double x, y;
        int strEditPoint;

        std::function<int(CMenu &, menuOpt &)> updateCppCallback;
        std::function<int(CMenu &, menuOpt &)> posUpdateCppCallback;

        void update();

        inline menuOpt() {
            type = button_ok;
            group = -1;
            status = 0;
            x = y = 0.0;
            texture = 0;
            deslizantBarSize = 0.0;
            listID = 0;
            optValue = 0;
            devStatus = 0;
            preTextMaxSize = 30;

            attachedLua = nullptr;
            externalPreTextRef = nullptr;
            strEditPoint = 0;

            goback = false;
            btnClickStat = false;

            enableEnter = true;

            pressingShortcutKey = shortcutKey = 0;

            preText = "type here";
        }
    };

    ui::Typography resolvedOptionStyle(const menuOpt &option) const;

  private:
    Context *context = nullptr;
    CFonts &fontService() const;
    double aspect() const;
    double cursorX() const;
    double cursorY() const;
    void translate(menuOpt &);
    Rect optionRect(size_t index, const ui::Style &style);
    Rect controlRect(size_t index, const ui::Style &style);
    void reflow(const ui::Style &style);
    void refreshFocusGeometry(const ui::Style &style);
    void revealFocused(const ui::Style &style);

  public:
    Rect optionRect(size_t index);
    Rect controlRect(size_t index);
    void ensureLayout(menuOpt &);
    void revealFocused();
    // Freeze automatic positions before the editor moves an individual item.
    void beginManualEdit(size_t index);
    void select(int index);

    struct group {
        enum types { none, deslizant_h_menu };
        double x, y, h, w;
        double nx, ny;
        int status;
        int selectedOpt;
        int type;

        group() {
            nx = ny = x = y = h = w = 0.0;
            status = 0;
            selectedOpt = 0;
            type = 0;
        }
    };

    inline const std::string &getName() { return menuName; }

    std::map<int, group> groupInfo;
    std::deque<menuOpt> options;

    std::function<int(CMenu &)> openCallback;

    int pushUserInterface(const CMenu &m);

    size_t getDevSelectedMenuOpt();

    void update();
    void updateDev();
    void shortcutCallback(int key, int scancode, int action, int mods);

    void render();
    int addOpt(const menuOpt &opt);

    size_t getNumOfOptions() { return options.size(); }

    std::string getNameFromOpt(size_t optID) {
        return optID < options.size() ? options[optID].optionName
                                      : std::string();
    }

    void resetData();
    void resetBtns();

    int status;

    CMenu();
    CMenu(const std::string &name, Context *context = nullptr);
    CMenu(const CMenu &);
    CMenu &operator=(const CMenu &) = delete;
    ~CMenu();
};

#endif
